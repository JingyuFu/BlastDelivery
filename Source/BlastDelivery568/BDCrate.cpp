#include "BDCrate.h"
#include "BDVisuals.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

ABDCrate::ABDCrate()
{
    PrimaryActorTick.bCanEverTick = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeavyCrate"));
    SetRootComponent(Mesh);
    BDVisuals::Shape(Mesh, TEXT("Cube"));
    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetCollisionResponseToAllChannels(ECR_Block);
    Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Mesh->SetSimulatePhysics(true);
    Mesh->SetUseCCD(true);
    Mesh->CanCharacterStepUpOn = ECB_No;
    // A heavy delivery crate slides, but cannot tumble or gain vertical blast velocity.
    Mesh->BodyInstance.bLockXRotation = true;
    Mesh->BodyInstance.bLockYRotation = true;
    Mesh->BodyInstance.bLockZRotation = true;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        auto* Strap = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Strap%d"), Index));
        Strap->SetupAttachment(Mesh);
        BDVisuals::Shape(Strap, TEXT("Cube"));
        Strap->SetRelativeScale3D(FVector(.10f, 1.015f, 1.015f));
        Strap->SetRelativeLocation(FVector(Index == 0 ? -28 : 28, 0, 0));
        Straps.Add(Strap);
    }
}

void ABDCrate::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    BDVisuals::Material(Mesh, TEXT("Crate"));
    for (auto& Strap : Straps) BDVisuals::Material(Strap, TEXT("CrateBand"));
}

void ABDCrate::BeginPlay()
{
    Super::BeginPlay();
    SpawnTransform = GetActorTransform();
    Mesh->SetConstraintMode(EDOFMode::SixDOF);
    Mesh->SetMassOverrideInKg(NAME_None, MassKg);
    Mesh->SetLinearDamping(LinearDamping);
    FrictionMaterial = NewObject<UPhysicalMaterial>(this);
    FrictionMaterial->Friction = .22f;
    FrictionMaterial->StaticFriction = .22f;
    FrictionMaterial->Restitution = 0.f;
    FrictionMaterial->bOverrideFrictionCombineMode = true;
    FrictionMaterial->FrictionCombineMode = EFrictionCombineMode::Min;
    Mesh->SetPhysMaterialOverride(FrictionMaterial);
}

void ABDCrate::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const FVector V = Mesh->GetPhysicsLinearVelocity();
    if (V.Size2D() > MaxHorizontalSpeed)
    {
        const FVector XY = V.GetSafeNormal2D() * MaxHorizontalSpeed;
        Mesh->SetPhysicsLinearVelocity(FVector(XY.X,XY.Y,V.Z));
    }
    if (GetActorLocation().Z < -300.f) Respawn();
}

void ABDCrate::ApplyBlast(const FVector& Origin, float Strength)
{
    FVector Direction = (GetActorLocation() - Origin).GetSafeNormal2D();
    if (Direction.IsNearlyZero()) Direction = FVector::ForwardVector;
    Mesh->AddImpulse(Direction * Strength);
}

void ABDCrate::Respawn()
{
    Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
    Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
    Mesh->WakeAllRigidBodies();
}
