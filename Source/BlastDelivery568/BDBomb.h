#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BDBomb.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class ABDCharacter;

UCLASS()
class BLASTDELIVERY568_API ABDBomb : public AActor
{
    GENERATED_BODY()
public:
    ABDBomb();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void Ignite(ABDCharacter* Character, float Fuse);
    void Release(bool bThrow);
    void Nudge(const FVector& Direction);
    void Explode();
    bool IsHeld() const { return Holder.IsValid(); }
    float GetRemainingFuse() const { return RemainingFuse; }
    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FuseTip;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;
private:
    TWeakObjectPtr<ABDCharacter> Holder;
    float RemainingFuse = 2.5f;
    bool bExploded = false;
};
