#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BDCrate.generated.h"

class UStaticMeshComponent;
class UPhysicalMaterial;

UCLASS()
class BLASTDELIVERY568_API ABDCrate : public AActor
{
    GENERATED_BODY()
public:
    ABDCrate();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;
    void ApplyBlast(const FVector& Origin, float Strength);
    void Respawn();
    FVector GetSpawnPosition() const { return SpawnTransform.GetLocation(); }
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(EditAnywhere, Category="Physics") float MassKg = 120.f;
    UPROPERTY(EditAnywhere, Category="Physics") float MaxHorizontalSpeed = 600.f;
    UPROPERTY(EditAnywhere, Category="Physics") float LinearDamping = .65f;
private:
    UPROPERTY() TObjectPtr<UPhysicalMaterial> FrictionMaterial;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Straps;
    FTransform SpawnTransform;
};
