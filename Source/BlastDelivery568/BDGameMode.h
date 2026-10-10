#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BDGameMode.generated.h"

class ABDBomb;
class ABDCharacter;
class ABDCrate;
class ABDGoal;
class ABDGate;

UCLASS(Config=Game)
class BLASTDELIVERY568_API ABDGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ABDGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditDefaultsOnly, Config, Category="Bomb") float FuseSeconds = 2.5f;
    UPROPERTY(EditDefaultsOnly, Config, Category="Bomb") float BlastRadius = 360.f;
    UPROPERTY(EditDefaultsOnly, Config, Category="Bomb") float CrateImpulse = 110000.f;
    UPROPERTY(EditDefaultsOnly, Config, Category="Bomb") float ThrowSpeed = 650.f;
    UPROPERTY(EditDefaultsOnly, Config, Category="Bomb") float ThrowLift = 260.f;
    UPROPERTY(EditDefaultsOnly, Config, Category="Jump") float JumpHorizontalSpeed = 740.f;
    UPROPERTY(EditDefaultsOnly, Config, Category="Jump") float JumpVerticalSpeed = 700.f;
    UPROPERTY(EditDefaultsOnly, Config, Category="Goal") float SettleSeconds = 0.8f;

    UPROPERTY(BlueprintReadOnly, Category="Run") int32 BombsUsed = 0;
    UPROPERTY(BlueprintReadOnly, Category="Run") float ElapsedSeconds = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="Run") bool bCompleted = false;
    UPROPERTY(BlueprintReadOnly, Category="Run") TObjectPtr<ABDCrate> Crate;
    UPROPERTY(BlueprintReadOnly, Category="Run") TObjectPtr<ABDGoal> Goal;
    UPROPERTY(BlueprintReadOnly, Category="Run") TObjectPtr<ABDGate> Gate;
    UPROPERTY(BlueprintReadOnly, Category="Run") TObjectPtr<ABDBomb> ActiveBomb;

    ABDBomb* DrawBomb(ABDCharacter* Holder);
    void ClearBomb(ABDBomb* Bomb);
    void ExplodeAt(const FVector& Position, ABDCharacter* Holder, AActor* Bomb);
    void RestartTutorial();
    void CompleteTutorial();
    static float BlastFalloff(float Distance, float Radius);
    bool HasBlastLineOfSight(const FVector& From, AActor* Target, AActor* Bomb) const;
private:
    float SettledFor = 0.f;
};
