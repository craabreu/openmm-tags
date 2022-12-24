/* -------------------------------------------------------------------------- *
 *                          OpenMM Nonbonded Slicing                          *
 *                          ========================                          *
 *                                                                            *
 * An OpenMM plugin for slicing nonbonded potential calculations on the basis *
 * of atom pairs and for applying scaling parameters to selected slices.      *
 *                                                                            *
 * Copyright (c) 2022 Charlles Abreu                                          *
 * https://github.com/craabreu/openmm-nonbonded-slicing                       *
 * -------------------------------------------------------------------------- */

#include "SlicedPmeForce.h"
#include "internal/SlicedPmeForceImpl.h"
#include "openmm/Force.h"
#include "openmm/OpenMMException.h"
#include "openmm/internal/AssertionUtilities.h"
#include <cmath>
#include <map>
#include <sstream>
#include <utility>
#include <algorithm>

using namespace NonbondedSlicing;
using namespace OpenMM;
using std::map;
using std::pair;
using std::set;
using std::string;
using std::stringstream;
using std::vector;

#define ASSERT_VALID_SUBSET(subset) {if (subset < 0 || subset >= numSubsets) throwException(__FILE__, __LINE__, "Subset out of range");};

SlicedPmeForce::SlicedPmeForce(int numSubsets) : numSubsets(numSubsets), cutoffDistance(1.0),
        ewaldErrorTol(5e-4), alpha(0.0), dalpha(0.0), exceptionsUsePeriodic(false), recipForceGroup(-1),
        includeDirectSpace(true), nx(0), ny(0), nz(0), dnx(0), dny(0), dnz(0), useCudaFFT(false) {
    if (numSubsets <= 0)
        throw OpenMMException("Invalid number of subsets");
    for (int j = 0; j < numSubsets*(numSubsets+1)/2; j++)
        switchingParameter.push_back(1.0);
}

SlicedPmeForce::SlicedPmeForce(const NonbondedForce& force, int numSubsets) : SlicedPmeForce(numSubsets) {
    NonbondedForce::NonbondedMethod method = force.getNonbondedMethod();
    if (method == NonbondedForce::NoCutoff || method == NonbondedForce::CutoffNonPeriodic)
        throw OpenMMException("SlicedPmeForce: cannot instantiate from a non-periodic NonbondedForce");
    cutoffDistance = force.getCutoffDistance();
    ewaldErrorTol = force.getEwaldErrorTolerance();
    force.getPMEParameters(alpha, nx, ny, nz);
    exceptionsUsePeriodic = force.getExceptionsUsePeriodicBoundaryConditions();
    recipForceGroup = force.getReciprocalSpaceForceGroup();
    includeDirectSpace = force.getIncludeDirectSpace();

    for (int index = 0; index < force.getNumParticles(); index++) {
        double charge, sigma, epsilon;
        force.getParticleParameters(index, charge, sigma, epsilon);
        addParticle(charge);
    }

    for (int index = 0; index < force.getNumExceptions(); index++) {
        int particle1, particle2;
        double chargeProd, sigma, epsilon;
        force.getExceptionParameters(index, particle1, particle2, chargeProd, sigma, epsilon);
        addException(particle1, particle2, chargeProd);
    }

    for (int index = 0; index < force.getNumGlobalParameters(); index++)
        addGlobalParameter(force.getGlobalParameterName(index),
                           force.getGlobalParameterDefaultValue(index));

    for (int index = 0; index < force.getNumParticleParameterOffsets(); index++) {
        std::string parameter;
        int particleIndex;
        double chargeScale, sigmaScale, epsilonScale;
        force.getParticleParameterOffset(index, parameter, particleIndex, chargeScale, sigmaScale, epsilonScale);
        addParticleChargeOffset(parameter, particleIndex, chargeScale);
    }

    for (int index = 0; index < force.getNumExceptionParameterOffsets(); index++) {
        std::string parameter;
        int exceptionIndex;
        double chargeProdScale, sigmaScale, epsilonScale;
        force.getExceptionParameterOffset(index, parameter, exceptionIndex, chargeProdScale, sigmaScale, epsilonScale);
        addExceptionChargeOffset(parameter, exceptionIndex, chargeProdScale);
    }
}

double SlicedPmeForce::getCutoffDistance() const {
    return cutoffDistance;
}

void SlicedPmeForce::setCutoffDistance(double distance) {
    cutoffDistance = distance;
}

double SlicedPmeForce::getEwaldErrorTolerance() const {
    return ewaldErrorTol;
}

void SlicedPmeForce::setEwaldErrorTolerance(double tol) {
    ewaldErrorTol = tol;
}

void SlicedPmeForce::getPMEParameters(double& alpha, int& nx, int& ny, int& nz) const {
    alpha = this->alpha;
    nx = this->nx;
    ny = this->ny;
    nz = this->nz;
}

void SlicedPmeForce::setPMEParameters(double alpha, int nx, int ny, int nz) {
    this->alpha = alpha;
    this->nx = nx;
    this->ny = ny;
    this->nz = nz;
}

void SlicedPmeForce::getPMEParametersInContext(const Context& context, double& alpha, int& nx, int& ny, int& nz) const {
    dynamic_cast<const SlicedPmeForceImpl&>(getImplInContext(context)).getPMEParameters(alpha, nx, ny, nz);
}

int SlicedPmeForce::addParticle(double charge, int subset) {
    ASSERT_VALID_SUBSET(subset);
    particles.push_back(ParticleInfo(charge, subset));
    return particles.size()-1;
}

int SlicedPmeForce::getParticleSubset(int index) const {
    ASSERT_VALID_INDEX(index, particles);
    return particles[index].subset;
}

void SlicedPmeForce::setParticleSubset(int index, int subset) {
    ASSERT_VALID_INDEX(index, particles);
    ASSERT_VALID_SUBSET(subset);
    particles[index].subset = subset;
}

double SlicedPmeForce::getParticleCharge(int index) const {
    ASSERT_VALID_INDEX(index, particles);
    return particles[index].charge;
}

void SlicedPmeForce::setParticleCharge(int index, double charge) {
    ASSERT_VALID_INDEX(index, particles);
    particles[index].charge = charge;
}

int SlicedPmeForce::addException(int particle1, int particle2, double chargeProd, bool replace) {
    map<pair<int, int>, int>::iterator iter = exceptionMap.find(pair<int, int>(particle1, particle2));
    int newIndex;
    if (iter == exceptionMap.end())
        iter = exceptionMap.find(pair<int, int>(particle2, particle1));
    if (iter != exceptionMap.end()) {
        if (!replace) {
            stringstream msg;
            msg << "SlicedPmeForce: There is already an exception for particles ";
            msg << particle1;
            msg << " and ";
            msg << particle2;
            throw OpenMMException(msg.str());
        }
        exceptions[iter->second] = ExceptionInfo(particle1, particle2, chargeProd);
        newIndex = iter->second;
        exceptionMap.erase(iter->first);
    }
    else {
        exceptions.push_back(ExceptionInfo(particle1, particle2, chargeProd));
        newIndex = exceptions.size()-1;
    }
    exceptionMap[pair<int, int>(particle1, particle2)] = newIndex;
    return newIndex;
}

void SlicedPmeForce::getExceptionParameters(int index, int& particle1, int& particle2, double& chargeProd) const {
    ASSERT_VALID_INDEX(index, exceptions);
    particle1 = exceptions[index].particle1;
    particle2 = exceptions[index].particle2;
    chargeProd = exceptions[index].chargeProd;
}

void SlicedPmeForce::setExceptionParameters(int index, int particle1, int particle2, double chargeProd) {
    ASSERT_VALID_INDEX(index, exceptions);
    exceptions[index].particle1 = particle1;
    exceptions[index].particle2 = particle2;
    exceptions[index].chargeProd = chargeProd;
}

ForceImpl* SlicedPmeForce::createImpl() const {
    return new SlicedPmeForceImpl(*this);
}

void SlicedPmeForce::createExceptionsFromBonds(const vector<pair<int, int> >& bonds, double coulomb14Scale, double lj14Scale) {
    for (auto& bond : bonds)
        if (bond.first < 0 || bond.second < 0 || bond.first >= particles.size() || bond.second >= particles.size())
            throw OpenMMException("createExceptionsFromBonds: Illegal particle index in list of bonds");

    // Find particles separated by 1, 2, or 3 bonds.

    vector<set<int> > exclusions(particles.size());
    vector<set<int> > bonded12(exclusions.size());
    for (auto& bond : bonds) {
        bonded12[bond.first].insert(bond.second);
        bonded12[bond.second].insert(bond.first);
    }
    for (int i = 0; i < (int) exclusions.size(); ++i)
        addExclusionsToSet(bonded12, exclusions[i], i, i, 2);

    // Find particles separated by 1 or 2 bonds and create the exceptions.

    for (int i = 0; i < (int) exclusions.size(); ++i) {
        set<int> bonded13;
        addExclusionsToSet(bonded12, bonded13, i, i, 1);
        for (int j : exclusions[i]) {
            if (j < i) {
                if (bonded13.find(j) == bonded13.end()) {
                    // This is a 1-4 interaction.

                    const ParticleInfo& particle1 = particles[j];
                    const ParticleInfo& particle2 = particles[i];
                    const double chargeProd = coulomb14Scale*particle1.charge*particle2.charge;
                    addException(j, i, chargeProd);
                }
                else {
                    // This interaction should be completely excluded.

                    addException(j, i, 0.0);
                }
            }
        }
    }
}

void SlicedPmeForce::addExclusionsToSet(const vector<set<int> >& bonded12, set<int>& exclusions, int baseParticle, int fromParticle, int currentLevel) const {
    for (int i : bonded12[fromParticle]) {
        if (i != baseParticle)
            exclusions.insert(i);
        if (currentLevel > 0)
            addExclusionsToSet(bonded12, exclusions, baseParticle, i, currentLevel-1);
    }
}

int SlicedPmeForce::addGlobalParameter(const string& name, double defaultValue) {
    globalParameters.push_back(GlobalParameterInfo(name, defaultValue));
    return globalParameters.size()-1;
}

const string& SlicedPmeForce::getGlobalParameterName(int index) const {
    ASSERT_VALID_INDEX(index, globalParameters);
    return globalParameters[index].name;
}

void SlicedPmeForce::setGlobalParameterName(int index, const string& name) {
    ASSERT_VALID_INDEX(index, globalParameters);
    globalParameters[index].name = name;
}

double SlicedPmeForce::getGlobalParameterDefaultValue(int index) const {
    ASSERT_VALID_INDEX(index, globalParameters);
    return globalParameters[index].defaultValue;
}

void SlicedPmeForce::setGlobalParameterDefaultValue(int index, double defaultValue) {
    ASSERT_VALID_INDEX(index, globalParameters);
    globalParameters[index].defaultValue = defaultValue;
}

int SlicedPmeForce::getGlobalParameterIndex(const std::string& parameter) const {
    for (int i = 0; i < globalParameters.size(); i++)
        if (globalParameters[i].name == parameter)
            return i;
    throw OpenMMException("There is no global parameter called '"+parameter+"'");
}

int SlicedPmeForce::addSwitchingParameter(const std::string& parameter, int subset1, int subset2) {
    ASSERT_VALID_SUBSET(subset1);
    ASSERT_VALID_SUBSET(subset2);
    int i = std::min(subset1, subset2);
    int j = std::max(subset1, subset2);
    int slice = j*(j+1)/2+i;
    for (auto parameter : switchingParameters)
        if (parameter.slice == slice)
            throwException(__FILE__, __LINE__, "A switching parameter has already been defined for this slice");
    switchingParameters.push_back(SwitchingParameterInfo(getGlobalParameterIndex(parameter), subset1, subset2));
    return switchingParameters.size()-1;
}

int SlicedPmeForce::getNumSwitchingParameters() const {
    return switchingParameters.size();
}

void SlicedPmeForce::getSwitchingParameter(int index, std::string& parameter, int& subset1, int& subset2) const {
    ASSERT_VALID_INDEX(index, switchingParameters);
    int globalParamIndex = switchingParameters[index].globalParamIndex;
    parameter = globalParameters[globalParamIndex].name;
    subset1 = switchingParameters[index].subset1;
    subset2 = switchingParameters[index].subset2;
}

void SlicedPmeForce::setSwitchingParameter(int index, const std::string& parameter, int subset1, int subset2) {
    ASSERT_VALID_INDEX(index, switchingParameters);
    ASSERT_VALID_SUBSET(subset1);
    ASSERT_VALID_SUBSET(subset2);
    int i = std::min(subset1, subset2);
    int j = std::max(subset1, subset2);
    int slice = j*(j+1)/2+i;
    int globalParamIndex = getGlobalParameterIndex(parameter);
    if (switchingParameters[index].slice != slice)
        for (auto parameter : switchingParameters)
            if (parameter.slice == slice)
                throwException(__FILE__, __LINE__, "A switching parameter has already been defined for this slice");
    switchingParameters[index] = SwitchingParameterInfo(globalParamIndex, subset1, subset2);
}

int SlicedPmeForce::getSwitchingParameterIndex(const std::string& parameter) const {
    for (int index = 0; index < switchingParameters.size(); index++) {
        int globalParamIndex = switchingParameters[index].globalParamIndex;
        if (globalParameters[globalParamIndex].name == parameter)
            return index;
    }
    throw OpenMMException("There is no switching parameter called '"+parameter+"'");
}

int SlicedPmeForce::addSwitchingParameterDerivative(const string& parameter) {
    int switchParamIndex = getSwitchingParameterIndex(parameter);
    auto begin = switchParamDerivatives.begin();
    auto end = switchParamDerivatives.end();
    if (find(begin, end, switchParamIndex) != end)
        throwException(__FILE__, __LINE__, "This switching parameter derivative was already requested");
    switchParamDerivatives.push_back(switchParamIndex);
    return switchParamDerivatives.size()-1;
}

int SlicedPmeForce::getNumSwitchingParameterDerivatives() const {
    return switchParamDerivatives.size();
}

const string& SlicedPmeForce::getSwitchingParameterDerivativeName(int index) const {
    ASSERT_VALID_INDEX(index, switchParamDerivatives);
    int switchParamIndex = switchParamDerivatives[index];
    int globalParamIndex = switchingParameters[switchParamIndex].globalParamIndex;
    return globalParameters[globalParamIndex].name;
}

void SlicedPmeForce::setSwitchingParameterDerivative(int index, const string& parameter) {
    ASSERT_VALID_INDEX(index, switchParamDerivatives);
    int switchParamIndex = getSwitchingParameterIndex(parameter);
    if (switchParamDerivatives[index] != switchParamIndex) {
        auto begin = switchParamDerivatives.begin();
        auto end = switchParamDerivatives.end();
        if (find(begin, end, switchParamIndex) != end)
            throwException(__FILE__, __LINE__, "This switching parameter derivative was already requested");
        switchParamDerivatives[index] = switchParamIndex;
    }
}

int SlicedPmeForce::addParticleChargeOffset(const std::string& parameter, int particleIndex, double chargeScale) {
    particleOffsets.push_back(ParticleOffsetInfo(getGlobalParameterIndex(parameter), particleIndex, chargeScale));
    return particleOffsets.size()-1;
}

void SlicedPmeForce::getParticleChargeOffset(int index, std::string& parameter, int& particleIndex, double& chargeScale) const {
    ASSERT_VALID_INDEX(index, particleOffsets);
    parameter = globalParameters[particleOffsets[index].parameter].name;
    particleIndex = particleOffsets[index].particle;
    chargeScale = particleOffsets[index].chargeScale;
}

void SlicedPmeForce::setParticleChargeOffset(int index, const std::string& parameter, int particleIndex, double chargeScale) {
    ASSERT_VALID_INDEX(index, particleOffsets);
    particleOffsets[index].parameter = getGlobalParameterIndex(parameter);
    particleOffsets[index].particle = particleIndex;
    particleOffsets[index].chargeScale = chargeScale;
}

int SlicedPmeForce::addExceptionChargeOffset(const std::string& parameter, int exceptionIndex, double chargeProdScale) {
    exceptionOffsets.push_back(ExceptionOffsetInfo(getGlobalParameterIndex(parameter), exceptionIndex, chargeProdScale));
    return exceptionOffsets.size()-1;
}

void SlicedPmeForce::getExceptionChargeOffset(int index, std::string& parameter, int& exceptionIndex, double& chargeProdScale) const {
    ASSERT_VALID_INDEX(index, exceptionOffsets);
    parameter = globalParameters[exceptionOffsets[index].parameter].name;
    exceptionIndex = exceptionOffsets[index].exception;
    chargeProdScale = exceptionOffsets[index].chargeProdScale;
}

void SlicedPmeForce::setExceptionChargeOffset(int index, const std::string& parameter, int exceptionIndex, double chargeProdScale) {
    ASSERT_VALID_INDEX(index, exceptionOffsets);
    exceptionOffsets[index].parameter = getGlobalParameterIndex(parameter);
    exceptionOffsets[index].exception = exceptionIndex;
    exceptionOffsets[index].chargeProdScale = chargeProdScale;
}

int SlicedPmeForce::getReciprocalSpaceForceGroup() const {
    return recipForceGroup;
}

void SlicedPmeForce::setReciprocalSpaceForceGroup(int group) {
    if (group < -1 || group > 31)
        throw OpenMMException("Force group must be between -1 and 31");
    recipForceGroup = group;
}

bool SlicedPmeForce::getIncludeDirectSpace() const {
    return includeDirectSpace;
}

void SlicedPmeForce::setIncludeDirectSpace(bool include) {
    includeDirectSpace = include;
}

void SlicedPmeForce::updateParametersInContext(Context& context) {
    dynamic_cast<SlicedPmeForceImpl&>(getImplInContext(context)).updateParametersInContext(getContextImpl(context));
}

bool SlicedPmeForce::getExceptionsUsePeriodicBoundaryConditions() const {
    return exceptionsUsePeriodic;
}

void SlicedPmeForce::setExceptionsUsePeriodicBoundaryConditions(bool periodic) {
    exceptionsUsePeriodic = periodic;
}
