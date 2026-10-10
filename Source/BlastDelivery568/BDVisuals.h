#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

// All meshes ship with Unreal. Palette assets are made by Scripts/create_tutorial.py.
namespace BDVisuals
{
inline void Material(UStaticMeshComponent* Mesh, const TCHAR* Name)
{
    const FString Path = FString::Printf(TEXT("/Game/Materials/M_%s.M_%s"), Name, Name);
    if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *Path))
        Mesh->SetMaterial(0, Mat);
}
inline void Shape(UStaticMeshComponent* Mesh, const TCHAR* Name)
{
    const FString Path = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *Path));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetGenerateOverlapEvents(false);
}
}
