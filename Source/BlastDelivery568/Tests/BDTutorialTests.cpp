#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "../BDCharacter.h"
#include "../BDCrate.h"
#include "../BDBomb.h"
#include "../BDGameMode.h"
#include "../BDPlayerController.h"
#include "../BDPuzzleActors.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "InputKeyEventArgs.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"

namespace BDTests
{
struct FFixture
{
    FAutomationTestBase* Test;
    UWorld* World = nullptr;
    ABDGameMode* Mode = nullptr;
    ABDCharacter* Courier = nullptr;
    ABDPlayerController* Controller = nullptr;
    FVector SavedPosition;
    FVector SavedDirection;
    float SavedFuse = 0.f;
    float SavedTime = 0.f;
    float NearSpeed = 0.f;
    void Refresh()
    {
        World = GEditor->PlayWorld;
        if (!World) return;
        Mode = World->GetAuthGameMode<ABDGameMode>();
        Courier = Cast<ABDCharacter>(UGameplayStatics::GetPlayerPawn(World,0));
        Controller = Cast<ABDPlayerController>(UGameplayStatics::GetPlayerController(World,0));
    }
    void Key(FKey Key, EInputEvent Event) const
    {
        Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key,Event, Event == IE_Released ? 0.f : 1.f));
        // Dispatch each simulated edge separately. Consecutive latent steps can run
        // in one editor frame; queuing release+press together misrepresents a click.
        Controller->PlayerTick(Controller->IsPaused() ? 0.f : 1.f/120.f);
    }
    void PlacePlayer(FVector P, FRotator R = FRotator::ZeroRotator) const
    {
        Courier->GetCharacterMovement()->StopMovementImmediately();
        Courier->SetActorLocationAndRotation(P,R,false,nullptr,ETeleportType::TeleportPhysics);
        Courier->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    }
    void PlaceCrate(FVector P) const
    {
        Mode->Crate->Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        Mode->Crate->Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        Mode->Crate->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);
        Mode->Crate->Mesh->WakeAllRigidBodies();
    }
    void ClearBomb() const { if (IsValid(Mode->ActiveBomb)) Mode->ActiveBomb->Destroy(); }
};
using FAction = TFunction<void(FFixture&)>;

class FStep : public IAutomationLatentCommand
{
    TSharedRef<FFixture> Fixture;
    FString Name;
    float Duration;
    FAction Begin, During, End;
    double Started = -1;
    double WaitingSince = FPlatformTime::Seconds();
public:
    FStep(TSharedRef<FFixture> InFixture, FString InName, float Seconds, FAction InBegin, FAction InDuring, FAction InEnd)
        : Fixture(InFixture), Name(MoveTemp(InName)), Duration(Seconds), Begin(MoveTemp(InBegin)), During(MoveTemp(InDuring)), End(MoveTemp(InEnd)) {}
    virtual bool Update() override
    {
        Fixture->Refresh();
        if (!Fixture->World || !Fixture->Mode || !Fixture->Courier || !Fixture->Controller)
        {
            if (FPlatformTime::Seconds()-WaitingSince > 20) { Fixture->Test->AddError(TEXT("PIE failed to start/reload")); return true; }
            return false;
        }
        if (Started < 0)
        {
            Fixture->Test->AddInfo(TEXT("STEP: ") + Name);
            UE_LOG(LogTemp, Display, TEXT("BD TEST: %s"), *Name);
            Started = FPlatformTime::Seconds();
            if (Begin) Begin(*Fixture);
        }
        if (During) During(*Fixture);
        if (FPlatformTime::Seconds()-Started < Duration) return false;
        if (End) End(*Fixture);
        return true;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBDTutorialTest, "BlastDelivery.Tutorial.GameplayAndPuzzle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBDTutorialTest::RunTest(const FString& Parameters)
{
    using namespace BDTests;
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Maps/Tutorial")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    auto F = MakeShared<FFixture>();
    F->Test = this;
    auto Step = [&](const TCHAR* Name,float Seconds,FAction Begin={},FAction During={},FAction End={})
    { FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStep>(F,Name,Seconds,MoveTemp(Begin),MoveTemp(During),MoveTemp(End))); };

    Step(TEXT("Saved map starts with all puzzle actors"), .7f, {}, {}, [](FFixture& X)
    {
        X.Test->TestNotNull(TEXT("Crate"),X.Mode->Crate.Get());
        X.Test->TestNotNull(TEXT("Goal"),X.Mode->Goal.Get());
        X.Test->TestNotNull(TEXT("Gate"),X.Mode->Gate.Get());
        X.Test->TestFalse(TEXT("Gate starts closed"),X.Mode->Gate->bOpen);
        X.SavedPosition=X.Courier->GetActorLocation();
        X.Key(EKeys::D,IE_Pressed);
    });
    Step(TEXT("WASD through controller input"), .45f, {}, {}, [](FFixture& X)
    {
        X.Key(EKeys::D,IE_Released);
        X.Test->TestTrue(TEXT("D moves continuously right"),X.Courier->GetActorLocation().X > X.SavedPosition.X+70);
        X.Key(EKeys::RightMouseButton,IE_Pressed);
    });
    Step(TEXT("RMB draws and ignites"), .3f, {}, {}, [](FFixture& X)
    {
        X.Test->TestNotNull(TEXT("RMB spawned bomb"),X.Mode->ActiveBomb.Get());
        if (!X.Mode->ActiveBomb) return;
        X.Test->TestTrue(TEXT("Held fuse already burning"),X.Mode->ActiveBomb->GetRemainingFuse()<2.45f);
        X.SavedFuse=X.Mode->ActiveBomb->GetRemainingFuse();
        X.SavedTime=X.Mode->ElapsedSeconds;
        X.Controller->TogglePause();
    });
    Step(TEXT("Pause freezes fuse, timer and crate physics"), .4f, [](FFixture& X){X.SavedPosition=X.Mode->Crate->GetActorLocation();}, {}, [](FFixture& X)
    {
        if (X.Mode->ActiveBomb) X.Test->TestEqual(TEXT("Frozen fuse"),X.Mode->ActiveBomb->GetRemainingFuse(),X.SavedFuse);
        X.Test->TestEqual(TEXT("Frozen timer"),X.Mode->ElapsedSeconds,X.SavedTime);
        X.Test->TestTrue(TEXT("Frozen physics"),X.Mode->Crate->GetActorLocation().Equals(X.SavedPosition,.01));
        X.Controller->TogglePause();
        X.Key(EKeys::RightMouseButton,IE_Released);
    });
    Step(TEXT("Release drops without restarting fuse"), .3f, {}, {}, [](FFixture& X)
    {
        X.Test->TestTrue(TEXT("Dropped bomb remains active"),IsValid(X.Mode->ActiveBomb));
        if (X.Mode->ActiveBomb)
        {
            X.Test->TestFalse(TEXT("No longer held"),X.Mode->ActiveBomb->IsHeld());
            X.Test->TestTrue(TEXT("Fuse continues"),X.Mode->ActiveBomb->GetRemainingFuse()<X.SavedFuse-.1f);
        }
        const int32 Count=X.Mode->BombsUsed;
        X.Courier->DrawBomb();
        X.Test->TestEqual(TEXT("One bomb at a time"),X.Mode->BombsUsed,Count);
    });
    Step(TEXT("Dropped bomb detonates and supply remains unlimited"), 2.2f, {}, {}, [](FFixture& X)
    {
        X.Test->TestFalse(TEXT("Spent bomb cleared"),IsValid(X.Mode->ActiveBomb));
        X.Controller->SetActorTickEnabled(false);
        X.PlacePlayer(FVector(-700,150,66));
        X.Courier->DrawBomb();
        X.SavedFuse=X.Mode->ActiveBomb->GetRemainingFuse();
        X.SavedPosition=X.Mode->ActiveBomb->GetActorLocation();
        FVector2D AimScreen;
        if (X.Controller->ProjectWorldLocationToScreen(X.Courier->GetActorLocation()+FVector(400,0,0),AimScreen))
            X.Controller->SetMouseLocation(FMath::RoundToInt(AimScreen.X),FMath::RoundToInt(AimScreen.Y));
        X.Controller->PressBomb();
        // Throw uses the current cursor, including when it moved since the last tick.
        X.Controller->ThrowBomb();
        X.SavedDirection=X.Courier->GetActorForwardVector();
    });
    Step(TEXT("LMB throw travels toward facing and keeps fuse"), .35f, {}, {}, [](FFixture& X)
    {
        X.Test->TestFalse(TEXT("Thrown bomb detached"),X.Mode->ActiveBomb->IsHeld());
        X.Test->TestTrue(TEXT("Throw moves toward cursor"),FVector::DotProduct(X.Mode->ActiveBomb->GetActorLocation()-X.SavedPosition,X.SavedDirection)>90);
        X.Test->TestTrue(TEXT("Throw fuse continues"),X.Mode->ActiveBomb->GetRemainingFuse()<X.SavedFuse-.15f);
        X.ClearBomb();
        X.PlacePlayer(FVector(-650,-100,66));
        X.SavedPosition=X.Mode->Crate->GetActorLocation();
    });
    Step(TEXT("Walking cannot push crate"), 1.1f, {}, [](FFixture& X){X.Courier->MoveInRoom(FVector2D(1,0));}, [](FFixture& X)
    {
        X.Test->TestTrue(TEXT("Crate unchanged under walking contact"),FVector::Dist2D(X.SavedPosition,X.Mode->Crate->GetActorLocation())<.5f);
        X.PlacePlayer(FVector(-720,-100,66));
        X.Courier->LaunchCharacter(FVector(1100,0,0),true,true);
    });
    Step(TEXT("Launched player cannot push crate"), 1.2f, {}, {}, [](FFixture& X)
    {
        X.Test->TestTrue(TEXT("Crate unchanged under launched contact"),FVector::Dist2D(X.SavedPosition,X.Mode->Crate->GetActorLocation())<.5f);
        X.PlacePlayer(FVector(-750,-100,66));
        X.Courier->DrawBomb();
        X.Courier->DropBomb();
        X.Mode->ActiveBomb->SetActorLocation(FVector(-600,-100,20));
        X.Mode->ActiveBomb->Nudge(FVector(1,0,0));
    });
    Step(TEXT("Loose bomb contact cannot push crate"), .7f, {}, {}, [](FFixture& X)
    {
        X.Test->TestTrue(TEXT("Crate unchanged by bomb contact"),FVector::Dist2D(X.SavedPosition,X.Mode->Crate->GetActorLocation())<.5f);
        X.ClearBomb();
        X.PlacePlayer(FVector(-780,500,66));
        X.Mode->ExplodeAt(X.Mode->Crate->GetActorLocation()-FVector(150,0,0),nullptr,nullptr);
    });
    Step(TEXT("Explosion applies real crate impulse"), .15f, {}, {}, [](FFixture& X)
    {
        X.NearSpeed=X.Mode->Crate->Mesh->GetPhysicsLinearVelocity().Size2D();
        X.Test->TestTrue(TEXT("Explosion moves crate"),X.NearSpeed>100);
        X.Mode->Crate->Respawn();
    });
    Step(TEXT("Prepare distant explosion"), .3f, {}, {}, [](FFixture& X){X.Mode->ExplodeAt(X.Mode->Crate->GetActorLocation()-FVector(300,0,0),nullptr,nullptr);});
    Step(TEXT("Impulse weakens with distance"), .15f, {}, {}, [](FFixture& X)
    {
        X.Test->TestTrue(TEXT("Far impulse weaker than near"),X.Mode->Crate->Mesh->GetPhysicsLinearVelocity().Size2D()<X.NearSpeed*.6f);
        X.Mode->Crate->Respawn();
        X.PlacePlayer(FVector(-270,-715,66),FRotator(0,180,0));
        X.Courier->DrawBomb();
        X.Mode->ActiveBomb->Explode();
    });
    Step(TEXT("Closed gate blocks a full blast jump"), 1.3f, {}, {}, [](FFixture& X)
    {
        X.Test->TestTrue(TEXT("Player stays left of gate"),X.Courier->GetActorLocation().X < -175);
        X.PlacePlayer(FVector(-270,0,66),FRotator(0,180,0));
        X.Courier->DrawBomb(); X.Mode->ActiveBomb->Explode();
    });
    Step(TEXT("Solid L wall blocks a full blast jump"), 1.3f, {}, {}, [](FFixture& X)
    {
        X.Test->TestTrue(TEXT("Player stays left of wall"),X.Courier->GetActorLocation().X < -175);
        X.PlacePlayer(FVector(-700,500,66));
        X.PlaceCrate(FVector(-260,-715,53));
        X.Mode->Crate->ApplyBlast(FVector(-400,-715,53),200000);
    });
    Step(TEXT("Closed gate stops blasted crate"), 1.5f, {}, {}, [](FFixture& X)
    {
        X.Test->TestTrue(TEXT("Crate remains left of closed gate"),X.Mode->Crate->GetActorLocation().X < -199);
        X.PlaceCrate(FVector(100,675,53));
        X.Mode->Crate->ApplyBlast(FVector(-100,675,53),200000);
    });
    Step(TEXT("Crate cannot cross pit at maximum speed"), 2.2f, {}, {}, [](FFixture& X)
    {
        X.Test->TestTrue(TEXT("Crate fell and individually respawned"),FVector::Dist2D(X.Mode->Crate->GetActorLocation(),X.Mode->Crate->GetSpawnPosition())<1.f);
        X.PlacePlayer(FVector(65,675,66),FRotator(0,180,0));
    });
    Step(TEXT("Launch pad settles"), .3f, {}, {}, [](FFixture& X){X.Courier->DrawBomb();});
    Step(TEXT("Hold entire fuse to cross pit"), 3.85f, {}, {}, [](FFixture& X)
    {
        X.Test->AddInfo(FString::Printf(TEXT("Held blast landing: %s"),*X.Courier->GetActorLocation().ToString()));
        X.Test->TestTrue(TEXT("Held bomb crosses full gap"),X.Courier->GetActorLocation().X>630);
        X.Test->TestTrue(TEXT("Safe grounded landing"),X.Courier->GetCharacterMovement()->IsMovingOnGround());
        X.Test->TestFalse(TEXT("Gate still requires plate"),X.Mode->Gate->bOpen);
    });
    Step(TEXT("Walk from landing north along right side"), 4.2f, {}, [](FFixture& X)
    {
        if (X.Courier->GetActorLocation().Y > -510) X.Courier->MoveInRoom(FVector2D(0,-1));
    });
    Step(TEXT("Walk left onto pressure plate"), 2.f, {}, [](FFixture& X)
    {
        if (X.Courier->GetActorLocation().X>300) X.Courier->MoveInRoom(FVector2D(-1,0));
    }, [](FFixture& X)
    {
        X.Test->TestTrue(TEXT("Stepping on plate opens gate"),X.Mode->Gate->bOpen);
        X.SavedPosition=X.Mode->Crate->GetActorLocation();
        X.PlacePlayer(FVector(400,675,66));
    });
    Step(TEXT("Player fall resets only player"), 1.7f, {}, {}, [](FFixture& X)
    {
        X.Test->TestTrue(TEXT("Player at own spawn"),FVector::Dist2D(X.Courier->GetActorLocation(),X.Courier->GetSpawnPosition())<1);
        X.Test->TestTrue(TEXT("Crate preserved"),FVector::Dist2D(X.Mode->Crate->GetActorLocation(),X.SavedPosition)<1);
        X.Test->TestTrue(TEXT("Gate persists after player fall"),X.Mode->Gate->bOpen);
        X.SavedPosition=X.Courier->GetActorLocation();
        X.PlaceCrate(FVector(400,675,53));
    });
    Step(TEXT("Crate fall resets only crate"), 1.7f, {}, {}, [](FFixture& X)
    {
        X.Test->TestTrue(TEXT("Crate at own spawn"),FVector::Dist2D(X.Mode->Crate->GetActorLocation(),X.Mode->Crate->GetSpawnPosition())<1);
        X.Test->TestTrue(TEXT("Player preserved"),FVector::Dist2D(X.Courier->GetActorLocation(),X.SavedPosition)<1);
        X.Test->TestTrue(TEXT("Gate persists after crate fall"),X.Mode->Gate->bOpen);
    });

    // Deliver the original crate through the real collision geometry using explosions
    // only. Each small impulse is produced by a real bomb at a reachable floor location.
    // Fixture positioning is restricted to the player/bomb; the crate is never teleported
    // between its spawn and the goal during this route check.
    for (int32 I=0; I<16; ++I)
    {
        Step(TEXT("Bomb-only delivery along upper passage"), 2.0f, [I](FFixture& X)
        {
            if (X.Mode->bCompleted) return;
            const FVector P=X.Mode->Crate->GetActorLocation();
            const bool bNorth=P.Y > -680.f && P.X < -300;
            const FVector Direction=bNorth?FVector(0,-1,0):FVector(1,0,0);
            const float Remaining=bNorth?P.Y+710.f:665.f-P.X;
            if (Remaining<15.f) return;
            const float DesiredStep=FMath::Min(Remaining,150.f);
            const float DesiredSpeed=FMath::Sqrt(430.f*DesiredStep)+.325f*DesiredStep;
            const float Distance=X.Mode->BlastRadius*(1.f-DesiredSpeed*120.f/X.Mode->CrateImpulse);
            const FVector BombPosition=P-Direction*FMath::Clamp(Distance,100.f,320.f);
            X.PlacePlayer(BombPosition-Direction*50.f+FVector(0,0,14),Direction.Rotation());
            X.Courier->DrawBomb();
            if (X.Mode->ActiveBomb) X.Mode->ActiveBomb->Explode();
            X.Test->AddInfo(FString::Printf(TEXT("Delivery blast %d: crate %s, bomb %s"),I,*P.ToString(),*BombPosition.ToString()));
        });
    }
    Step(TEXT("Complete physical delivery"), 1.f, {}, {}, [](FFixture& X)
    {
        X.Test->AddInfo(TEXT("Final crate position: ")+X.Mode->Crate->GetActorLocation().ToString());
        X.Test->TestTrue(TEXT("Bomb-only route reaches settled goal"),X.Mode->bCompleted);
        X.Controller->Restart();
    });
    Step(TEXT("Full restart restores puzzle and counters"), .8f, {}, {}, [](FFixture& X)
    {
        X.Controller->SetActorTickEnabled(false);
        X.Test->TestFalse(TEXT("Restart closes gate"),X.Mode->Gate->bOpen);
        X.Test->TestFalse(TEXT("Restart clears completion"),X.Mode->bCompleted);
        X.Test->TestFalse(TEXT("Restart removes bombs"),IsValid(X.Mode->ActiveBomb));
        X.Test->TestEqual(TEXT("Restart resets bomb count"),X.Mode->BombsUsed,0);
        X.Test->TestTrue(TEXT("Restart resets timer"),X.Mode->ElapsedSeconds<2.f);
        X.PlaceCrate(FVector(480,-650,53));
    });
    Step(TEXT("Partial overlap must not win"), 1.2f, {}, {}, [](FFixture& X)
    {
        X.Test->TestFalse(TEXT("Partial overlap rejected"),X.Mode->bCompleted);
        X.PlaceCrate(FVector(640,-650,53));
        X.Mode->Crate->Mesh->SetPhysicsLinearVelocity(FVector(200,0,0));
        X.Test->TestFalse(TEXT("Moving crate rejected"),X.Mode->Goal->ContainsSettledCrate(X.Mode->Crate));
    });
    Step(TEXT("Settle dwell resets when crate exits"), .25f, {}, {}, [](FFixture& X)
    {
        X.Test->TestFalse(TEXT("Passing through is not instant completion"),X.Mode->bCompleted);
        X.PlaceCrate(FVector(665,-650,53));
    });
    Step(TEXT("Fully contained and stopped wins"), 1.6f, {}, {}, [](FFixture& X)
    {
        X.Test->AddInfo(FString::Printf(TEXT("Settled goal check: position %s, velocity %s, bounds %s"),
            *X.Mode->Crate->GetActorLocation().ToString(), *X.Mode->Crate->Mesh->GetPhysicsLinearVelocity().ToString(),
            *X.Mode->Crate->Mesh->Bounds.GetBox().ToString()));
        X.Test->TestTrue(TEXT("Settled delivery completes"),X.Mode->bCompleted);
        X.SavedTime=X.Mode->ElapsedSeconds;
        X.Courier->DrawBomb();
        X.Test->TestFalse(TEXT("Completion disables new bombs"),IsValid(X.Mode->ActiveBomb));
    });
    Step(TEXT("Completion timer remains frozen"), .3f, {}, {}, [](FFixture& X)
    { X.Test->TestEqual(TEXT("Final time retained"),X.Mode->ElapsedSeconds,X.SavedTime); });
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBDBombReuseAndCameraTest, "BlastDelivery.Tutorial.BombReuseAndCamera",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBDBombReuseAndCameraTest::RunTest(const FString& Parameters)
{
    using namespace BDTests;
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Maps/Tutorial")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    auto F = MakeShared<FFixture>();
    F->Test = this;
    auto Step = [&](const TCHAR* Name,float Seconds,FAction Begin={},FAction During={},FAction End={})
    { FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStep>(F,Name,Seconds,MoveTemp(Begin),MoveTemp(During),MoveTemp(End))); };

    Step(TEXT("Mouse capture and angled camera"), .7f, [](FFixture& X)
    {
        X.World->GetGameViewport()->GetGameViewport()->SetFixedViewportSize(1280,800);
    }, {}, [](FFixture& X)
    {
        X.Test->TestTrue(TEXT("Every viewport mouse press is forwarded"),
            X.World->GetGameViewport()->GetMouseCaptureMode()==EMouseCaptureMode::CaptureDuringMouseDown);
        auto* Camera = Cast<ACameraActor>(X.Controller->GetViewTarget());
        X.Test->TestNotNull(TEXT("Fixed room camera"),Camera);
        if (!Camera) return;
        X.Test->TestTrue(TEXT("Pitch is -65 degrees"),FMath::IsNearlyEqual(Camera->GetActorRotation().Pitch,-65.0,.01));
        X.Test->TestTrue(TEXT("Room axes stay aligned"),FMath::IsNearlyEqual(Camera->GetActorRotation().Yaw,-90.0,.01));
        X.Test->TestTrue(TEXT("Orthographic projection"),Camera->GetCameraComponent()->ProjectionMode==ECameraProjectionMode::Orthographic);
        int32 W,H;
        X.Controller->GetViewportSize(W,H);
        for (double PX : {-980.0,980.0}) for (double PY : {-980.0,980.0}) for (double PZ : {0.0,130.0})
        {
            FVector2D Screen;
            const bool bProjected=X.Controller->ProjectWorldLocationToScreen(FVector(PX,PY,PZ),Screen);
            X.Test->TestTrue(TEXT("Room floor and wall corners stay in frame"),bProjected && Screen.X>0 && Screen.X<W && Screen.Y>0 && Screen.Y<H);
        }
        X.Mode->FuseSeconds=.65f;
    });
    // Repeat each full lifecycle through controller key dispatch, without manually
    // clearing the active reference. One round also exercises Slate double-click events.
    for (int32 Cycle=0;Cycle<6;++Cycle)
    {
        const int32 ReleaseMode=Cycle%3; // held, dropped, thrown
        Step(TEXT("Press RMB again after previous destruction"), .12f, [Cycle](FFixture& X)
        {
            X.PlacePlayer(FVector(-680,300,66));
            X.Key(EKeys::RightMouseButton,Cycle>=3 ? IE_DoubleClick : IE_Pressed);
        }, {}, [Cycle,ReleaseMode](FFixture& X)
        {
            X.Test->TestNotNull(TEXT("New bomb created"),X.Mode->ActiveBomb.Get());
            X.Test->TestEqual(TEXT("Counter increments exactly once per creation"),X.Mode->BombsUsed,Cycle+1);
            if (!IsValid(X.Mode->ActiveBomb)) return;
            X.Test->TestTrue(TEXT("New bomb starts held"),X.Mode->ActiveBomb->IsHeld());
            X.SavedFuse=X.Mode->ActiveBomb->GetRemainingFuse();
            X.Test->TestTrue(TEXT("Fuse starts immediately"),X.SavedFuse>0.f && X.SavedFuse<X.Mode->FuseSeconds);
            X.Controller->PressBomb();
            X.Test->TestEqual(TEXT("An active bomb blocks another creation"),X.Mode->BombsUsed,Cycle+1);
            if (ReleaseMode==1) X.Key(EKeys::RightMouseButton,IE_Released);
            if (ReleaseMode==2) X.Key(EKeys::LeftMouseButton,IE_Pressed);
        });
        Step(TEXT("Release keeps the same burning fuse"), .12f, {}, {}, [ReleaseMode](FFixture& X)
        {
            if (!X.Test->TestNotNull(TEXT("Bomb survives release"),X.Mode->ActiveBomb.Get())) return;
            X.Test->TestEqual(TEXT("Held state matches action"),X.Mode->ActiveBomb->IsHeld(),ReleaseMode==0);
            X.Test->TestTrue(TEXT("Fuse continues without resetting"),X.Mode->ActiveBomb->GetRemainingFuse()<X.SavedFuse);
            if (ReleaseMode==2)
            {
                X.Key(EKeys::LeftMouseButton,IE_Released);
                X.Key(EKeys::RightMouseButton,IE_Released);
            }
        });
        Step(TEXT("Detonation releases the active slot"), .85f, {}, {}, [Cycle](FFixture& X)
        {
            X.Test->TestNull(TEXT("Held/dropped/thrown explosion clears reference"),X.Mode->ActiveBomb.Get());
            X.Test->TestEqual(TEXT("Holding RMB does not auto-create after explosion"),X.Mode->BombsUsed,Cycle+1);
            X.Key(EKeys::RightMouseButton,IE_Released);
        });
    }
    Step(TEXT("Non-explosion destruction also permits reuse"), .12f, [](FFixture& X)
    {
        X.Mode->FuseSeconds=2.5f;
        X.PlacePlayer(FVector(-680,300,66));
        X.Key(EKeys::RightMouseButton,IE_Pressed);
    }, {}, [](FFixture& X)
    {
        if (!X.Test->TestNotNull(TEXT("Bomb before explicit destruction"),X.Mode->ActiveBomb.Get())) return;
        ABDBomb* OldBomb=X.Mode->ActiveBomb;
        OldBomb->Destroy();
        X.Test->TestNull(TEXT("EndPlay clears reference immediately"),X.Mode->ActiveBomb.Get());
        X.Controller->PressBomb();
        X.Test->TestNotNull(TEXT("Replacement can spawn immediately"),X.Mode->ActiveBomb.Get());
        X.Mode->ClearBomb(OldBomb);
        X.Test->TestNotNull(TEXT("Old cleanup cannot clear replacement"),X.Mode->ActiveBomb.Get());
        X.Test->TestEqual(TEXT("Both successful creations counted"),X.Mode->BombsUsed,8);
        X.ClearBomb();
        X.Key(EKeys::RightMouseButton,IE_Released);
    });
    Step(TEXT("Cursor ray follows the tilted camera"), .2f, [](FFixture& X)
    {
        X.PlacePlayer(FVector(-680,300,66));
        FVector2D Screen;
        if (X.Controller->ProjectWorldLocationToScreen(X.Courier->GetActorLocation()+FVector(400,-200,0),Screen))
            X.Controller->SetMouseLocation(FMath::RoundToInt(Screen.X),FMath::RoundToInt(Screen.Y));
    }, {}, [](FFixture& X)
    {
        const FVector Expected=FVector(400,-200,0).GetSafeNormal();
        X.Test->TestTrue(TEXT("Facing agrees with cursor at -65 degrees"),FVector::DotProduct(X.Courier->GetActorForwardVector(),Expected)>.999);
        X.Controller->PressBomb();
        if (!X.Test->TestNotNull(TEXT("Bomb for aim check"),X.Mode->ActiveBomb.Get())) return;
        // Move the cursor and throw immediately, before another PlayerTick.
        FVector2D Screen;
        if (X.Controller->ProjectWorldLocationToScreen(X.Courier->GetActorLocation()+FVector(300,200,0),Screen))
            X.Controller->SetMouseLocation(FMath::RoundToInt(Screen.X),FMath::RoundToInt(Screen.Y));
        X.Controller->ThrowBomb();
        X.Test->TestTrue(TEXT("Throw uses latest cursor direction"),FVector::DotProduct(
            X.Mode->ActiveBomb->Movement->Velocity.GetSafeNormal2D(),FVector(300,200,0).GetSafeNormal())>.999);
        X.ClearBomb();
    });
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
