#include "BDPuzzleActors.h"
#include "BDCharacter.h"
#include "BDCrate.h"
#include "BDVisuals.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ABDBarrier::ABDBarrier()
{
    Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("FullHeightCollision"));
    SetRootComponent(Collision);
    Collision->SetCollisionProfileName(TEXT("BlockAll"));
    Collision->SetGenerateOverlapEvents(false);
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisibleWall"));
    Mesh->SetupAttachment(Collision);
    BDVisuals::Shape(Mesh, TEXT("Cube"));
}
void ABDBarrier::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Configure(Size);
}
void ABDBarrier::Configure(FVector InSize)
{
    Size = InSize;
    // The visible low wall preserves the overhead view. Collision continues well above
    // the 175 cm jump apex and below the floor, so neither gates nor walls can be hopped.
    Collision->SetBoxExtent(FVector(Size.X*.5f, Size.Y*.5f, 2000.f));
    Mesh->SetRelativeScale3D(Size / 100.f);
    Mesh->SetRelativeLocation(FVector(0,0,Size.Z*.5f));
    BDVisuals::Material(Mesh, IsA<ABDGate>() ? TEXT("Gate") : TEXT("Wall"));
}
void ABDGate::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    BDVisuals::Material(Mesh, TEXT("Gate"));
}
void ABDGate::Open()
{
    if (bOpen) return;
    bOpen = true;
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetVisibility(false);
}

ABDPressurePlate::ABDPressurePlate()
{
    PrimaryActorTick.bCanEverTick = true;
    Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("PlateTrigger"));
    SetRootComponent(Trigger);
    Trigger->SetBoxExtent(FVector(80,80,24));
    Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plate"));
    Mesh->SetupAttachment(Trigger);
    BDVisuals::Shape(Mesh, TEXT("Cube"));
    Mesh->SetRelativeScale3D(FVector(1.6,1.6,.1));
    Mesh->SetRelativeLocation(FVector(0,0,5));
}
void ABDPressurePlate::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    BDVisuals::Material(Mesh, TEXT("Plate"));
}
void ABDPressurePlate::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bActivated) return;
    const auto* Player = Cast<ABDCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Player || !Player->GetCharacterMovement()->IsMovingOnGround()) return;
    const FVector Local = GetActorTransform().InverseTransformPosition(Player->GetActorLocation());
    // Feet must be on the plate; flying above it never activates the gate.
    if (FMath::Abs(Local.X) < 80.f && FMath::Abs(Local.Y) < 80.f && Local.Z < 90.f && Local.Z > 30.f)
    {
        bActivated = true;
        BDVisuals::Material(Mesh, TEXT("Goal"));
        Mesh->SetRelativeLocation(FVector(0,0,2));
        if (Gate) Gate->Open();
    }
}

ABDGoal::ABDGoal()
{
    Region = CreateDefaultSubobject<UBoxComponent>(TEXT("DeliveryRegion"));
    SetRootComponent(Region);
    Region->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GoalFloor"));
    Mesh->SetupAttachment(Region);
    BDVisuals::Shape(Mesh, TEXT("Cube"));
}
void ABDGoal::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Region->SetBoxExtent(Extent);
    Mesh->SetRelativeScale3D(FVector(Extent.X*.02f, Extent.Y*.02f, .06f));
    Mesh->SetRelativeLocation(FVector(0,0,-Extent.Z+3.f));
    BDVisuals::Material(Mesh, TEXT("Goal"));
}
bool ABDGoal::ContainsSettledCrate(const ABDCrate* Crate) const
{
    if (!Crate || !Crate->Mesh) return false;
    const FBox Bounds = Crate->Mesh->Bounds.GetBox();
    // Check all eight corners in goal-local space: a partial overlap never wins.
    for (int32 Corner = 0; Corner < 8; ++Corner)
    {
        const FVector P((Corner&1) ? Bounds.Max.X : Bounds.Min.X,
                        (Corner&2) ? Bounds.Max.Y : Bounds.Min.Y,
                        (Corner&4) ? Bounds.Max.Z : Bounds.Min.Z);
        const FVector Local = GetActorTransform().InverseTransformPosition(P);
        // Chaos rests with a small contact/penetration tolerance. Keep the visible
        // horizontal boundary exact, but don't reject a settled box for sub-cm floor slop.
        if (FMath::Abs(Local.X) > Extent.X || FMath::Abs(Local.Y) > Extent.Y || FMath::Abs(Local.Z) > Extent.Z + 2.f) return false;
    }
    return Crate->Mesh->GetPhysicsLinearVelocity().Size() <= MaxSettledSpeed
        && Crate->Mesh->GetPhysicsAngularVelocityInDegrees().Size() <= 3.f;
}

ABDBlastFlash::ABDBlastFlash()
{
    PrimaryActorTick.bCanEverTick = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flash"));
    SetRootComponent(Mesh);
    BDVisuals::Shape(Mesh, TEXT("Sphere"));
}
void ABDBlastFlash::BeginPlay()
{
    Super::BeginPlay();
    BDVisuals::Material(Mesh, TEXT("BombGlow"));
    SetLifeSpan(.18f);
}
void ABDBlastFlash::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Age += DeltaSeconds;
    Mesh->SetWorldScale3D(FVector(.3f + Age*8.f, .3f + Age*8.f, .08f));
}
