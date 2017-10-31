#include "SimulationData.h"

USimulationData::USimulationData()
{
	// TODO

	transform = *(new FTransform());
	engineRPM = 0.0f;
	velocity = *(new FVector());
	gear = 0;
}

USimulationData* USimulationData::MAKE(FTransform transform, float rpm, FVector vel, int g, TArray<FTransform> path, TArray<FVector> speeds)
{
	USimulationData* newSim = NewObject<USimulationData>();
	newSim->transform = transform;
	newSim->engineRPM = rpm;
	newSim->velocity = vel;
	newSim->gear = g;
	newSim->path = path;
	newSim->velocities = speeds;

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

TArray<FTransform> USimulationData::GetPath()
{
	return this->path;
}

TArray<FVector> USimulationData::GetVelocities()
{
	return this->velocities;
}
