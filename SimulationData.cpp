#include "SimulationData.h"
#include "Landmark.h"

USimulationData::USimulationData()
{
	bIsReady = false;
}

USimulationData::~USimulationData()
{

}

//NTODO: investigate copy constructors for TARRAY
USimulationData* USimulationData::MAKE(FTransform transform, int g, TArray<FTransform> path, TArray<FVector> speeds, TArray<float> rpms, TMap<int32, TArray<ALandmark*>> landmarks)
{
	//NTODO: try a smart pointer to keep this from garbage collection
	//alt: 
	TWeakObjectPtr<USimulationData> newsim(NewObject<USimulationData>());
	//USimulationData* newSim = NewObject<USimulationData>();
	newsim->transform = transform;
	newsim->gear = g;
	newsim->path = path;
	newsim->velocities = speeds;
	newsim->rpms = rpms;
	newsim->bIsReady = true;
	newsim->landmarks = landmarks;
	newsim->AddToRoot();

	return newsim.Get();
}

void USimulationData::Initialize(FTransform tran, int g, TArray<FTransform> path, TArray<FVector> velocities, TArray<float> rpms, TMap<int32, TArray<ALandmark*>> landmarks)
{
	this->transform = transform;
	this->gear = g;
	this->path = path;
	this->velocities = velocities;
	this->rpms = rpms;
	this->bIsReady = true;
	this->landmarks = landmarks;

}

FTransform USimulationData::GetTransform()
{
	return this->transform;
}

//NTODO: return value of array (ret: Tarray<FTransform>)
TArray<FTransform> USimulationData::GetPath()
{
	return this->path;
}

TArray<FVector> USimulationData::GetVelocities()
{
	return this->velocities;
}

//NTODO: Return value of array
TArray<float> USimulationData::GetRMPValues()
{
	return this->rpms;
}

//NTODO: return a success bool, and pass in pre-created array by ptr to fill
TArray<ALandmark*> USimulationData::GetLandmarksAtTick(int32 tick)
{
	// TODO add fail case for key not exist?
	return this->landmarks[tick];
}

bool USimulationData::hasLandmarksAtTick(int32 tick)
{
	TArray<int32>* keys = new TArray<int32>;
	this->landmarks.GetKeys(*keys);
	return keys->Contains(tick);
	//NTODO: mem dealloc
}
