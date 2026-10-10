#include "BDPlayerController.h"
#include "BDCharacter.h"
#include "BDGameMode.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"

ABDPlayerController::ABDPlayerController()
{
    bShowMouseCursor = true;
    bAutoManageActiveCameraTarget = false;
    PrimaryActorTick.bTickEvenWhenPaused = true;
}
void ABDPlayerController::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(TEXT("TutorialCamera"))) { RoomCamera = *It; break; }
    if (RoomCamera)
    {
        // Apply this to the saved map's camera too; no map regeneration is required.
        const FRotator Rotation(OverheadPitch, -90.f, 0.f);
        RoomCamera->SetActorLocationAndRotation(ViewCenter - Rotation.Vector() * CameraDistance, Rotation);
        UCameraComponent* Camera = RoomCamera->GetCameraComponent();
        Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
        Camera->SetConstraintAspectRatio(false);
        Camera->bOverrideAspectRatioAxisConstraint = true;
        Camera->SetAspectRatioAxisConstraint(AspectRatio_MaintainXFOV);
        Camera->SetAutoCalculateOrthoPlanes(false);
        Camera->SetUpdateOrthoPlanes(false);
        Camera->SetOrthoNearClipPlane(1.f);
        Camera->SetOrthoFarClipPlane(6000.f);
        UpdateCameraFraming();
        SetViewTarget(RoomCamera);
    }
    FInputModeGameOnly Mode;
    Mode.SetConsumeCaptureMouseDown(false);
    SetInputMode(Mode);
    if (GetWorld()->GetGameViewport())
    {
        // NoCapture skips ordinary mouse-down dispatch once the initial capture is
        // released. Temporary capture forwards every press, including RMB + LMB.
        GetWorld()->GetGameViewport()->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);
        GetWorld()->GetGameViewport()->SetMouseLockMode(EMouseLockMode::DoNotLock);
        GetWorld()->GetGameViewport()->SetHideCursorDuringCapture(false);
    }
}
void ABDPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ABDPlayerController::PressBomb);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &ABDPlayerController::ReleaseBomb).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ABDPlayerController::ThrowBomb);
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ABDPlayerController::TogglePause).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ABDPlayerController::Restart).bExecuteWhenPaused = true;
}
void ABDPlayerController::PlayerTick(float DeltaSeconds)
{
    Super::PlayerTick(DeltaSeconds);
    UpdateCameraFraming();
    if (IsPaused()) return;
    auto* Mode = GetWorld()->GetAuthGameMode<ABDGameMode>();
    auto* Courier = Cast<ABDCharacter>(GetPawn());
    if (!Courier || !Mode || Mode->bCompleted) return;
    const FVector2D Input((IsInputKeyDown(EKeys::D) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::A) ? 1.f : 0.f),
                          (IsInputKeyDown(EKeys::S) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::W) ? 1.f : 0.f));
    Courier->MoveInRoom(Input);
    UpdateMouseAim(Courier);
    // Reconcile a release during pause or a focus change without advancing the fuse.
    if (bBombButtonDown && !IsInputKeyDown(EKeys::RightMouseButton)) ReleaseBomb();
}
void ABDPlayerController::UpdateCameraFraming()
{
    if (!RoomCamera) return;
    int32 W, H;
    GetViewportSize(W,H);
    const float Aspect = W > 0 && H > 0 ? float(W)/H : 16.f/9.f;
    RoomCamera->GetCameraComponent()->SetOrthoWidth(FMath::Max(MinimumViewWidth, MinimumViewHeight * Aspect));
}
void ABDPlayerController::UpdateMouseAim(ABDCharacter* Courier)
{
    // Intersect the cursor ray with the character's horizontal aiming plane.
    // This stays aligned with its on-screen position at any camera pitch/height,
    // including a blast jump, and avoids aiming at the invisible tall wall colliders.
    FVector RayStart, RayDirection;
    if (DeprojectMousePositionToWorld(RayStart,RayDirection) && FMath::Abs(RayDirection.Z) > SMALL_NUMBER)
    {
        const float Distance = (Courier->GetActorLocation().Z - RayStart.Z) / RayDirection.Z;
        if (Distance >= 0.f) Courier->FacePoint(RayStart + Distance * RayDirection);
    }
}
void ABDPlayerController::PressBomb()
{
    if (IsPaused()) return;
    bBombButtonDown = true;
    if (auto* Courier = Cast<ABDCharacter>(GetPawn()))
    {
        UpdateMouseAim(Courier);
        Courier->DrawBomb();
    }
}
void ABDPlayerController::ReleaseBomb()
{
    bBombButtonDown = false;
    if (!IsPaused()) if (auto* Courier = Cast<ABDCharacter>(GetPawn())) Courier->DropBomb();
}
void ABDPlayerController::ThrowBomb()
{
    if (bBombButtonDown && !IsPaused())
        if (auto* Courier = Cast<ABDCharacter>(GetPawn()))
        {
            // Input callbacks run before PlayerTick's usual aim update.
            UpdateMouseAim(Courier);
            Courier->ThrowBomb();
        }
}
void ABDPlayerController::TogglePause()
{
    const bool bWasPaused = IsPaused();
    SetPause(!bWasPaused);
    if (bWasPaused && !IsInputKeyDown(EKeys::RightMouseButton))
    {
        bBombButtonDown = false;
        if (auto* Courier = Cast<ABDCharacter>(GetPawn())) Courier->DropBomb();
    }
}
void ABDPlayerController::Restart()
{
    if (auto* Mode = GetWorld()->GetAuthGameMode<ABDGameMode>()) Mode->RestartTutorial();
}
