#include "SimulationData.h"

USimulationData::USimulationData()
{
	bIsReady = false;
}

USimulationData* USimulationData::MAKE(FTransform transform, float rpm, FVector vel, int g, TArray<FTransform> path, TArray<FVector> speeds, TArray<float> rpms, TArray<FName> landmarks)
{
	USimulationData* newSim = NewObject<USimulationData>();
	newSim->transform = transform;
	newSim->engineRPM = rpm;
	newSim->velocity = vel;
	newSim->gear = g;
	newSim->path = path;
	newSim->velocities = speeds;
	newSim->rpms = rpms;
	newSim->bIsReady = true;
	newSim->landmarks = landmarks;

	return newSim;
}

float USimulationData::calculateError(USimulationData& expected)
{
	FTransform trans = this->GetTransform();
	FVector veldiff = (expected.GetVelocity()) - (this->GetVelocity()); //TODO this won't compile as is
	veldiff = veldiff.GetAbs();
	FVector transdiff = expected.GetTransform().GetTranslation() - trans.GetTranslation();
	transdiff = transdiff.GetAbs();
	float rotdiff = expected.GetTransform().GetRotation().AngularDistance(trans.GetRotation());

	return veldiff.Size() + transdiff.Size() + rotdiff;
}

FTransform USimulationData::GetTransform()
{
	return this->transform;
}

FVector USimulationData::GetVelocity()
{
	return this->velocity;
}

TArray<FTransform>* USimulationData::GetPath()
{
	return &this->path;
}

TArray<FVector> USimulationData::GetVelocities()
{
	return this->velocities;
}

TArray<float>* USimulationData::GetRMPValues()
{
	return &this->rpms;
}

TArray<FName>* USimulationData::GetLandmarks()
{

	return &this->landmarks;
}
