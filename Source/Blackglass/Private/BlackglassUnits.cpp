#include "BlackglassGame.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavAreas/NavArea_Null.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "DrawDebugHelpers.h"

namespace {
UMaterialInterface* CorporateSurface() {
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Surface(
  TEXT("/Game/Materials/M_BlackglassSurface.M_BlackglassSurface"));
 static bool Warned = false;
 if (!Surface.Object && !Warned) {
  UE_LOG(LogTemp, Warning, TEXT("BLACKGLASS authored surface material is missing; primitive material fallback is a placeholder."));
  Warned = true;
 }
 return Surface.Object;
}
void ColorPart(UStaticMeshComponent* Part, const FLinearColor& Color, float Roughness = .8f) {
 if (!Part) return;
 UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0));
 if (!Material) Material = Part->CreateDynamicMaterialInstance(0);
 if (Material) {
  Material->SetVectorParameterValue(TEXT("Color"), Color);
  Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
  Material->SetScalarParameterValue(TEXT("DetailStrength"), 0.f);
 }
}
AAIController* UnitAI(ABGUnit* Unit) { return Unit ? Cast<AAIController>(Unit->GetController()) : nullptr; }
FVector UnitGround(const ABGUnit* Unit) {
 return Unit->GetActorLocation() - FVector(0, 0, Unit->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}
ABGOperation* WorldOperation(const AActor* Actor) {
 return Actor ? Cast<ABGOperation>(UGameplayStatics::GetGameMode(Actor)) : nullptr;
}
bool ClearExit(ABGUnit* Unit, const FVector& Location, ABGVehicle* Vehicle) {
 FCollisionQueryParams Params(SCENE_QUERY_STAT(BlackglassExit), false, Unit);
 Params.AddIgnoredActor(Vehicle);
 const auto* Capsule = Unit->GetCapsuleComponent();
 return !Unit->GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_Pawn,
  FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Params);
}
}

ABGUnit::ABGUnit() {
 PrimaryActorTick.bCanEverTick = true;
 AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
 AIControllerClass = AAIController::StaticClass();
 bUseControllerRotationYaw = false;
 GetCapsuleComponent()->InitCapsuleSize(30, 88);
 GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
 GetCapsuleComponent()->SetCanEverAffectNavigation(false);
 GetCharacterMovement()->bOrientRotationToMovement = true;
 GetCharacterMovement()->RotationRate = FRotator(0, 720, 0);
 GetCharacterMovement()->MaxWalkSpeed = 285;
 GetCharacterMovement()->MaxStepHeight = 40;
 GetCharacterMovement()->bUseRVOAvoidance = true;
 GetCharacterMovement()->AvoidanceConsiderationRadius = 280;
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
 auto Part = [&](const TCHAR* Name, UStaticMesh* Geometry, FVector Location, FVector Scale) {
  UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
  Component->SetupAttachment(GetCapsuleComponent());
  Component->SetStaticMesh(Geometry);
  if (UMaterialInterface* Surface = CorporateSurface()) Component->SetMaterial(0, Surface);
  Component->SetRelativeLocation(Location);
  Component->SetRelativeScale3D(Scale);
  Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Component->SetCanEverAffectNavigation(false);
  Parts.Add(Component);
  return Component;
 };
 Part(TEXT("CoatTorso"), Cube.Object, FVector(0, 0, 12), FVector(.30f, .40f, .70f));
 Part(TEXT("CoatSkirt"), Cube.Object, FVector(-3, -12, -41), FVector(.28f, .21f, .58f));
 Part(TEXT("Head"), Cube.Object, FVector(0, 0, 70), FVector(.20f, .22f, .28f));
 Part(TEXT("Collar"), Cube.Object, FVector(-2, 0, 50), FVector(.24f, .30f, .14f));
 LeftLeg = Part(TEXT("LeftLeg"), Cube.Object, FVector(0, -11, -62), FVector(.17f, .16f, .44f));
 RightLeg = Part(TEXT("RightLeg"), Cube.Object, FVector(0, 11, -62), FVector(.17f, .16f, .44f));
 LeftArm = Part(TEXT("LeftArm"), Cube.Object, FVector(0, -27, 7), FVector(.14f, .14f, .56f));
 RightArm = Part(TEXT("RightArm"), Cube.Object, FVector(0, 27, 7), FVector(.14f, .14f, .56f));
 WeaponMesh = Part(TEXT("Weapon"), Cube.Object, FVector(28, 25, 8), FVector(.38f, .09f, .11f));
 Part(TEXT("CorporateBadge"), Cube.Object, FVector(17, -13, 23), FVector(.025f, .10f, .06f));
 SelectionRing = Part(TEXT("SelectionMarker"), Cylinder.Object, FVector(0, 0, -87), FVector(.88f, .88f, .012f));
 SelectionRing->SetVisibility(false);
 SelectionRing->SetCastShadow(false);
 Part(TEXT("LeftShoulder"), Cube.Object, FVector(-2,-24,39), FVector(.26f,.14f,.17f));
 Part(TEXT("RightShoulder"), Cube.Object, FVector(-2,24,39), FVector(.26f,.14f,.17f));
 Part(TEXT("RightCoatTail"), Cube.Object, FVector(-3,12,-41), FVector(.28f,.21f,.58f));
 Part(TEXT("Hair"), Cube.Object, FVector(-.5f,0,84), FVector(.21f,.235f,.08f));
 auto* Eye = Part(TEXT("EyeAugment"), Cube.Object, FVector(10.5f,-5,75), FVector(.025f,.055f,.03f));
 Eye->SetCastShadow(false);
 auto* LapelLeft = Part(TEXT("LeftLapel"), Cube.Object, FVector(17,-8,26), FVector(.035f,.12f,.34f));
 auto* LapelRight = Part(TEXT("RightLapel"), Cube.Object, FVector(17,8,26), FVector(.035f,.12f,.34f));
 LapelLeft->SetCastShadow(false); LapelRight->SetCastShadow(false);
 Part(TEXT("LeftBoot"), Cube.Object, FVector(8,-11,-82), FVector(.26f,.18f,.12f));
 Part(TEXT("RightBoot"), Cube.Object, FVector(8,11,-82), FVector(.26f,.18f,.12f));
 auto* Grip = Part(TEXT("WeaponGrip"), Cube.Object, FVector::ZeroVector, FVector(.095f,.075f,.19f));
 Grip->SetupAttachment(WeaponMesh);
 Grip->SetAbsolute(false,false,true);
 Grip->SetRelativeLocation(FVector(-10,0,-100));
 Grip->SetCastShadow(false);
 Inventory.SetNum(8);
}

void ABGUnit::BeginPlay() {
 Super::BeginPlay();
 if (!GetController()) SpawnDefaultController();
 UpdateAppearance();
}
void ABGUnit::Initialize(int32 NewId, EBGRole NewRole, const FString& NewLabel) {
 EntityId = NewId; UnitRole = NewRole; Label = NewLabel;
 Inventory.SetNum(8);
 if (ABGOperation* Op = Operation()) {
  if (UnitRole == EBGRole::Operative || UnitRole == EBGRole::Guard) {
   const FName Primary = UnitRole == EBGRole::Guard ? FName(TEXT("compact_automatic")) : FName(TEXT("compact_sidearm"));
   if (const FBGWeaponDefinition* Def = Op->FindWeapon(Primary)) Inventory[0] = {Def->Id, Def->Magazine, Def->Reserve};
   if (UnitRole == EBGRole::Operative) {
    if (const FBGWeaponDefinition* Def = Op->FindWeapon(TEXT("compact_automatic"))) Inventory[1] = {Def->Id, Def->Magazine, Def->Reserve};
   }
  }
 }
 GetCharacterMovement()->MaxWalkSpeed = UnitRole == EBGRole::Civilian ? 210 : 285;
 if (!GetController()) SpawnDefaultController();
 UpdateAppearance();
}
ABGOperation* ABGUnit::Operation() const { return WorldOperation(this); }
const FBGWeaponDefinition* ABGUnit::WeaponDefinition() const {
 const ABGOperation* Op = Operation();
 return Op && Inventory.IsValidIndex(WeaponIndex) ? Op->FindWeapon(Inventory[WeaponIndex].WeaponId) : nullptr;
}
float ABGUnit::InventoryWeight() const {
 float Weight = 0;
 if (const ABGOperation* Op = Operation())
  for (const FBGItem& Item : Inventory) if (const auto* Def = Op->FindWeapon(Item.WeaponId)) Weight += Def->Weight;
 return Weight;
}
void ABGUnit::UpdateAppearance() {
 const FVector Poses[] = {FVector(0,0,12),FVector(-3,-12,-41),FVector(0,0,70),FVector(-2,0,50),
  FVector(0,-11,-62),FVector(0,11,-62),FVector(0,-27,7),FVector(0,27,7),FVector(28,25,8),FVector(17,-13,23),
  FVector(0,0,-87),FVector(-2,-24,39),FVector(-2,24,39),FVector(-3,12,-41),FVector(-.5f,0,84),
  FVector(10.5f,-5,75),FVector(17,-8,26),FVector(17,8,26),FVector(8,-11,-82),FVector(8,11,-82),FVector(-10,0,-100)};
 const bool LongCoat = UnitRole == EBGRole::Operative || UnitRole == EBGRole::Specialist;
 const FRotator Fall(0,0,85);
 for (int32 Index=0; Index<static_cast<int32>(UE_ARRAY_COUNT(Poses)) && Index<Parts.Num(); ++Index) {
  if (Parts[Index] == SelectionRing) continue;
  if (Index == 20) {
   Parts[Index]->SetRelativeLocation(Poses[Index]); Parts[Index]->SetRelativeRotation(FRotator::ZeroRotator); continue;
  }
  FVector Position = Poses[Index];
  if ((Index == 1 || Index == 13) && !LongCoat) Position.Z = -28;
  FRotator DetailRotation = FRotator::ZeroRotator;
  if (Index == 11) DetailRotation.Roll = -8;
  if (Index == 12) DetailRotation.Roll = 8;
  if (Index == 16) DetailRotation.Roll = -16;
  if (Index == 17) DetailRotation.Roll = 16;
  Parts[Index]->SetRelativeLocation(Alive() ? Position : Fall.RotateVector(Position)+FVector(0,0,-78));
  Parts[Index]->SetRelativeRotation(Alive() ? DetailRotation : (Fall.Quaternion()*DetailRotation.Quaternion()).Rotator());
 }
 const FLinearColor CivicColors[] = {FLinearColor(.32f,.22f,.12f),FLinearColor(.34f,.16f,.10f),
  FLinearColor(.17f,.26f,.20f),FLinearColor(.19f,.24f,.29f)};
 const FLinearColor SkinColors[] = {FLinearColor(.53f,.36f,.25f),FLinearColor(.33f,.20f,.13f),
  FLinearColor(.65f,.48f,.35f),FLinearColor(.43f,.30f,.21f)};
 const int32 Palette = FMath::Abs(EntityId)%4;
 const FLinearColor Coat = UnitRole == EBGRole::Operative ? FLinearColor(.045f,.070f,.080f) :
  UnitRole == EBGRole::Guard ? FLinearColor(.20f,.080f,.055f) :
  UnitRole == EBGRole::Specialist ? FLinearColor(.62f,.60f,.50f) : CivicColors[Palette];
 const FLinearColor Accent = UnitRole == EBGRole::Operative ? FLinearColor(.10f,.57f,.68f) :
  UnitRole == EBGRole::Guard ? FLinearColor(.66f,.22f,.085f) : FLinearColor(.64f,.58f,.32f);
 for (UStaticMeshComponent* Part : Parts) ColorPart(Part, Coat);
 if (Parts.Num() >= 21) {
  ColorPart(Parts[2], SkinColors[Palette], .65f);
  ColorPart(Parts[3], FLinearColor(.58f,.58f,.49f), .85f);
  ColorPart(LeftLeg, FLinearColor(.06f,.065f,.07f));
  ColorPart(RightLeg, FLinearColor(.06f,.065f,.07f));
  ColorPart(Parts[9], Accent, .4f);
  ColorPart(Parts[14], FLinearColor(.027f,.021f,.018f), .85f);
  ColorPart(Parts[15], Accent*.7f, .28f);
  Parts[15]->SetVisibility(UnitRole == EBGRole::Operative || UnitRole == EBGRole::Guard);
  ColorPart(Parts[16], Coat*1.6f, .74f); ColorPart(Parts[17], Coat*1.6f, .74f);
  ColorPart(Parts[18], FLinearColor(.022f,.025f,.028f), .48f);
  ColorPart(Parts[19], FLinearColor(.022f,.025f,.028f), .48f);
  Parts[1]->SetRelativeScale3D(FVector(.28f,.21f,LongCoat ? .58f:.32f));
  Parts[13]->SetRelativeScale3D(FVector(.28f,.21f,LongCoat ? .58f:.32f));
  ColorPart(Parts[20], FLinearColor(.025f,.029f,.035f), .36f);
 }
 ColorPart(WeaponMesh, FLinearColor(.035f,.043f,.050f), .32f);
 ColorPart(SelectionRing, FLinearColor(.075f,.58f,.72f), .7f);
 const auto* Definition = WeaponDefinition();
 const bool WeaponVisible = Alive() && !IsSeated() && !bHolstered && Definition != nullptr;
 WeaponMesh->SetVisibility(WeaponVisible);
 WeaponMesh->SetRelativeScale3D(FVector(Definition && Definition->Id == TEXT("compact_automatic") ? .5f : .30f, .09f, .11f));
 if (Parts.IsValidIndex(20)) Parts[20]->SetVisibility(WeaponVisible);
 SelectionRing->SetVisibility(bSelection && Alive() && !IsSeated());
 if (!Alive()) for (UStaticMeshComponent* Part : Parts) ColorPart(Part, Coat*.55f);
}
void ABGUnit::SetSelected(bool Selected) { bSelection = Selected; SelectionRing->SetVisibility(Selected && Alive() && !IsSeated()); }
FVector ABGUnit::EffectiveLocation() const {
 if (const ABGOperation* Op = Operation()) {
  if (VehicleId && Op->Vehicle && Op->Vehicle->EntityId == VehicleId) return Op->Vehicle->GetActorLocation();
 }
 return GetActorLocation();
}
void ABGUnit::Tick(float DeltaSeconds) {
 Super::Tick(DeltaSeconds);
 ABGOperation* Op = Operation();
 if (!Op || Op->bRestoring || Op->Outcome != EBGOutcome::Active) return;
 ShotCooldown = FMath::Max(-.5f, ShotCooldown - DeltaSeconds);
 if (bHasOrder && Order.Type == EBGOrderType::Move && Order.NavigationRetryStarted)
  Order.NavigationRetryRemaining = FMath::Max(0.f, Order.NavigationRetryRemaining - DeltaSeconds);
 Recoil = FMath::Max(0.f, Recoil - DeltaSeconds * 9);
 if (ReloadRemaining > 0) {
  ReloadRemaining = FMath::Max(0.f, ReloadRemaining - DeltaSeconds);
  if (ReloadRemaining <= 0 && Inventory.IsValidIndex(WeaponIndex))
   if (const auto* Def = WeaponDefinition()) {
    FBGItem& Item = Inventory[WeaponIndex];
    const int32 Count = FMath::Min(FMath::Max(0, Def->Magazine - Item.Ammo), Item.Reserve);
    Item.Ammo += Count; Item.Reserve -= Count;
   }
 }
 if (!Alive()) return;
 if (IsSeated()) return;
 const float Speed = GetVelocity().Size2D();
 GaitPhase += DeltaSeconds * FMath::Clamp(Speed / 32.f, 0.f, 11.f);
 const float Swing = FMath::Sin(GaitPhase) * FMath::Clamp(Speed / 8.f, 0.f, 27.f);
 LeftLeg->SetRelativeRotation(FRotator(Swing, 0, 0));
 RightLeg->SetRelativeRotation(FRotator(-Swing, 0, 0));
 const bool Aiming = !bHolstered && WeaponDefinition() != nullptr;
 LeftArm->SetRelativeLocation(FVector(0,-27,Aiming ? 30.f : 7.f));
 RightArm->SetRelativeLocation(FVector(0,27,Aiming ? 32.f : 7.f));
 LeftArm->SetRelativeRotation(FRotator(Aiming ? -50.f : -Swing * .6f, 0, 0));
 RightArm->SetRelativeRotation(FRotator(Aiming ? -65.f + Recoil * 22 : Swing * .6f, 0, 0));
 WeaponMesh->SetRelativeLocation(FVector(28 - Recoil * 8, 25, Aiming ? 20 : 8));
 if (Parts.IsValidIndex(19)) {
  Parts[18]->SetRelativeLocation(LeftLeg->GetRelativeLocation()+LeftLeg->GetRelativeRotation().RotateVector(FVector(8,0,-20)));
  Parts[18]->SetRelativeRotation(LeftLeg->GetRelativeRotation());
  Parts[19]->SetRelativeLocation(RightLeg->GetRelativeLocation()+RightLeg->GetRelativeRotation().RotateVector(FVector(8,0,-20)));
  Parts[19]->SetRelativeRotation(RightLeg->GetRelativeRotation());
  Parts[1]->SetRelativeRotation(FRotator(Swing*.12f,0,0));
  Parts[13]->SetRelativeRotation(FRotator(-Swing*.12f,0,0));
 }
 ThinkAccumulator += DeltaSeconds;
 if (ThinkAccumulator >= .2f) {
  const float Elapsed = ThinkAccumulator; ThinkAccumulator = 0; Think(Elapsed);
 }
 AActor* Target = bHasOrder && Order.Type == EBGOrderType::Attack ? Op->FindEntity(Order.TargetId) :
  UnitRole == EBGRole::Guard && bHostile && EvidenceSeconds > 0 ? Op->FindUnit(ThreatId) : nullptr;
 ABGUnit* TargetUnit = Cast<ABGUnit>(Target);
 if (Target && (!TargetUnit || (TargetUnit->Alive() && !TargetUnit->IsSeated() && Op->CanSee(this, TargetUnit)))) {
  for (int32 Catchup = 0; Catchup < 4 && ShotCooldown <= 0; ++Catchup) {
   const int32 AmmoBefore = Inventory.IsValidIndex(WeaponIndex) ? Inventory[WeaponIndex].Ammo : 0;
   FireAtEntity(Target);
   if (!Inventory.IsValidIndex(WeaponIndex) || Inventory[WeaponIndex].Ammo == AmmoBefore || (TargetUnit && !TargetUnit->Alive())) break;
  }
 }
 ShotCooldown = FMath::Max(0.f, ShotCooldown);
}

void ABGUnit::IssueOrder(const FBGOrder& NewOrder, bool Append) {
 ABGOperation* Op = Operation();
 if (!Alive() || !Op || Op->Outcome != EBGOutcome::Active) return;
 if (IsSeated()) { if (UnitRole == EBGRole::Operative) Op->Notify(TEXT("Disembark before assigning on-foot orders.")); return; }
 FBGOrder Command = NewOrder;
 Command.NavigationRetryRemaining = 0;
 Command.NavigationRetryStarted = false;
 if (Command.Type == EBGOrderType::Move) {
  UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
  FNavLocation Projected;
  if (!Nav || Command.Location.ContainsNaN() || !Nav->ProjectPointToNavigation(Command.Location, Projected, FVector(95,95,160))) {
   if (UnitRole == EBGRole::Operative) Op->Notify(Label + TEXT(": destination is outside accessible navigation."));
   return;
  }
  Command.Location = Projected.Location;
 }
 if (Append && bHasOrder) {
  if (OrderQueue.Num() >= 64) { if (UnitRole == EBGRole::Operative) Op->Notify(TEXT("Order queue is full.")); return; }
  OrderQueue.Add(Command); return;
 }
 if (AAIController* AI = UnitAI(this)) AI->StopMovement();
 OrderQueue.Empty(); Order = Command; bHasOrder = true; bHold = false;
 ProcessOrder();
}
void ABGUnit::StopOrders(bool Hold) {
 bHasOrder = false; OrderQueue.Empty(); bHold = Hold;
 Order.NavigationRetryRemaining = 0; Order.NavigationRetryStarted = false;
 if (AAIController* AI = UnitAI(this)) AI->StopMovement();
 if (IsSeated()) if (ABGOperation* Op = Operation())
  if (Op->Vehicle && Op->Vehicle->Occupants.IsValidIndex(0) && Op->Vehicle->Occupants[0] == EntityId) Op->Vehicle->StopTravel();
}
void ABGUnit::SwitchWeapon() {
 if (!Alive() || Inventory.Num() == 0) return;
 if (ReloadRemaining > 0) { if (auto* Op = Operation()) Op->Notify(TEXT("Finish reloading before switching weapons.")); return; }
 for (int32 Offset = 1; Offset < Inventory.Num(); ++Offset) {
  const int32 Next = (WeaponIndex + Offset) % Inventory.Num();
  if (!Inventory[Next].WeaponId.IsNone()) { WeaponIndex = Next; UpdateAppearance(); return; }
 }
 if (auto* Op = Operation()) Op->Notify(TEXT("No second weapon equipped."));
}
void ABGUnit::Reload() {
 const auto* Def = WeaponDefinition();
 if (!Alive() || !Def || !Inventory.IsValidIndex(WeaponIndex)) return;
 FBGItem& Item = Inventory[WeaponIndex];
 if (ReloadRemaining > 0) return;
 if (Item.Ammo >= Def->Magazine) { if (auto* Op = Operation()) Op->Notify(TEXT("Magazine is already full.")); return; }
 if (Item.Reserve <= 0) { if (UnitRole == EBGRole::Operative) if (auto* Op = Operation()) Op->Notify(TEXT("No reserve ammunition.")); return; }
 ReloadRemaining = Def->ReloadSeconds;
}
bool ABGUnit::MoveTo(const FVector& Destination) {
 AAIController* AI = UnitAI(this);
 UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 if (!AI || !Nav || !Alive() || IsSeated()) return false;
 FNavLocation Projected;
 if (!Nav->ProjectPointToNavigation(Destination, Projected, FVector(95,95,160))) return false;
 return AI->MoveToLocation(Projected.Location, 35, false, true, false, false, nullptr, false) != EPathFollowingRequestResult::Failed;
}
void ABGUnit::ProcessOrder() {
 ABGOperation* Op = Operation();
 if (!Op || !bHasOrder || !Alive() || IsSeated() || UGameplayStatics::IsGamePaused(this)) return;
 bool Complete = false;
 if (Order.Type == EBGOrderType::Move) {
  if (FVector::DistSquared(UnitGround(this), Order.Location) < FMath::Square(60.f)) Complete = true;
  else if (AAIController* AI = UnitAI(this))
   if (AI->GetMoveStatus() == EPathFollowingStatus::Idle) {
    if (!MoveTo(Order.Location)) {
     UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
     const bool Updating = Nav && (Nav->HasDirtyAreasQueued() || Nav->IsNavigationBuildInProgress() ||
      Nav->GetNumRemainingBuildTasks() > 0 || UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(this));
     if (!Order.NavigationRetryStarted && Updating) {
      Order.NavigationRetryStarted = true; Order.NavigationRetryRemaining = 3.f;
      if (UnitRole == EBGRole::Operative) Op->Notify(Label + TEXT(": access changed; movement waits briefly for a clear route."));
      UE_LOG(LogTemp, Log, TEXT("BLACKGLASS move waiting for navigation: unit=%d goal=%s"), EntityId, *Order.Location.ToString());
      return;
     }
     if (Order.NavigationRetryStarted && Order.NavigationRetryRemaining > 0) return;
     if (UnitRole == EBGRole::Operative) Op->Notify(Label + TEXT(": no complete navigation route."));
     Complete = true;
    } else {
     Order.NavigationRetryStarted = false; Order.NavigationRetryRemaining = 0;
    }
   }
 } else if (Order.Type == EBGOrderType::Attack) {
  AActor* Target = Op->FindEntity(Order.TargetId);
  ABGUnit* TargetUnit = Cast<ABGUnit>(Target);
  ABGVehicle* TargetVehicle = Cast<ABGVehicle>(Target);
  ABGDoor* TargetDoor = Cast<ABGDoor>(Target);
  if (!Target || (TargetUnit && !TargetUnit->Alive()) || (TargetVehicle && TargetVehicle->Health <= 0) ||
   (TargetDoor && TargetDoor->Health <= 0)) Complete = true;
  else {
   bHolstered = false; UpdateAppearance();
   const auto* Def = WeaponDefinition();
   const FVector TargetLocation = TargetUnit ? TargetUnit->EffectiveLocation() : Target->GetActorLocation();
   if (!Def) { Op->Notify(Label + TEXT(": no weapon selected.")); Complete = true; }
   else if (FVector::Dist(EffectiveLocation(), TargetLocation) <= Def->Range && (!TargetUnit || Op->CanSee(this, TargetUnit))) {
    if (AAIController* AI = UnitAI(this)) AI->StopMovement();
    SetActorRotation(FRotator(0, (TargetLocation-EffectiveLocation()).Rotation().Yaw, 0));
    FireAtEntity(Target);
   } else if (!bHold) {
    if (AAIController* AI = UnitAI(this)) if (AI->GetMoveStatus() == EPathFollowingStatus::Idle) {
     const FVector Approach = TargetLocation + (EffectiveLocation()-TargetLocation).GetSafeNormal2D()*160;
     if (!MoveTo(Approach)) { Op->Notify(Label + TEXT(": target approach is obstructed.")); Complete = true; }
    }
   }
  }
 } else {
  AActor* Target = Op->FindEntity(Order.TargetId);
  if (!Target) { Op->Notify(TEXT("Interaction target no longer exists.")); Complete = true; }
  else if ((Cast<ABGVehicle>(Target) ? FVector::Dist(EffectiveLocation(), Target->GetActorLocation()) :
   FVector::Dist2D(EffectiveLocation(), Target->GetActorLocation())) > (Cast<ABGVehicle>(Target) ? 330.f : 180.f)) {
   if (AAIController* AI = UnitAI(this)) if (AI->GetMoveStatus() == EPathFollowingStatus::Idle) {
    const FVector Away = (EffectiveLocation()-Target->GetActorLocation()).GetSafeNormal2D();
    FVector Approach = Target->GetActorLocation()+Away*115.f;
    if (ABGVehicle* Car = Cast<ABGVehicle>(Target)) {
     FVector LocalAway = Car->GetActorRotation().UnrotateVector(Away);
     if (LocalAway.IsNearlyZero()) LocalAway = FVector(1,0,0);
     const FVector Footprint = Car->Hull->GetScaledBoxExtent()+FVector(15,15,0);
     const double EdgeX = Footprint.X / FMath::Max(FMath::Abs(LocalAway.X), .0001);
     const double EdgeY = Footprint.Y / FMath::Max(FMath::Abs(LocalAway.Y), .0001);
     const double Distance = FMath::Min(EdgeX, EdgeY)+GetCapsuleComponent()->GetScaledCapsuleRadius()+25;
     Approach = Car->GetActorLocation()+Car->GetActorRotation().RotateVector(LocalAway)*Distance;
    }
    if (!MoveTo(Approach)) { if (UnitRole == EBGRole::Operative) Op->Notify(Label + TEXT(": interaction route is obstructed.")); Complete = true; }
   }
  } else {
   if (AAIController* AI = UnitAI(this)) AI->StopMovement();
   if (ABGVehicle* Car = Cast<ABGVehicle>(Target)) Car->Board(this);
   else if (ABGDoor* Door = Cast<ABGDoor>(Target)) Door->Toggle();
   else if (ABGUnit* Person = Cast<ABGUnit>(Target)) Op->AcquireSpecialist(this);
   Complete = true;
  }
 }
 if (Complete) {
  bHasOrder = false;
  if (OrderQueue.Num()) { Order = OrderQueue[0]; OrderQueue.RemoveAt(0); bHasOrder = true; }
 }
}
void ABGUnit::Think(float Interval) {
 ABGOperation* Op = Operation();
 if (!Op || !Alive() || IsSeated()) return;
 EvidenceSeconds = FMath::Max(0.f, EvidenceSeconds - Interval);
 if (bHasOrder) { ProcessOrder(); return; }
 if (UnitRole == EBGRole::Operative || (UnitRole == EBGRole::Specialist && Op->bSpecialistAcquired)) return;
 if (UnitRole == EBGRole::Guard) {
  ABGUnit* Seen = nullptr;
  for (ABGUnit* Unit : Op->Units) {
   if (!Unit || !Unit->Alive() || Unit->UnitRole != EBGRole::Operative || Unit->IsSeated()) continue;
   const bool Criminal = (bHostile && Unit->EntityId == ThreatId) || !Unit->bHolstered;
   if (Criminal && FVector::Dist2D(GetActorLocation(), Unit->EffectiveLocation()) < 1700 && Op->CanSee(this, Unit)) {
    Seen = Unit; break;
   }
  }
  if (Seen) {
   ThreatId = Seen->EntityId; LastKnown = UnitGround(Seen); EvidenceSeconds = 20; bHostile = true;
   bHolstered = false; UpdateAppearance();
   const auto* Def = WeaponDefinition();
   if (Def && FVector::Dist2D(GetActorLocation(), Seen->EffectiveLocation()) <= Def->Range) {
    if (AAIController* AI = UnitAI(this)) AI->StopMovement();
    SetActorRotation(FRotator(0, (Seen->EffectiveLocation()-GetActorLocation()).Rotation().Yaw,0)); FireAt(Seen);
   } else if (!bHold) MoveTo(LastKnown);
   return;
  }
  if (EvidenceSeconds > 0) {
   if (!bHold && FVector::DistSquared2D(GetActorLocation(), LastKnown) > FMath::Square(80.f)) MoveTo(LastKnown);
   else if (AAIController* AI = UnitAI(this)) AI->StopMovement();
   return;
  }
 } else if (EvidenceSeconds > 0) {
  const FVector Away = (GetActorLocation()-LastKnown).GetSafeNormal2D();
  if (AAIController* AI = UnitAI(this)) if (AI->GetMoveStatus() == EPathFollowingStatus::Idle) {
   if (!MoveTo(UnitGround(this)+Away*650)) {
    FVector Safe;
    if (UNavigationSystemV1::K2_GetRandomReachablePointInRadius(this, UnitGround(this)+Away*250, Safe, 400)) MoveTo(Safe);
   }
  }
  return;
 }
 if (bHold || PatrolPoints.IsEmpty()) return;
 PatrolIndex = FMath::Clamp(PatrolIndex, 0, PatrolPoints.Num()-1);
 if (FVector::DistSquared2D(GetActorLocation(), PatrolPoints[PatrolIndex]) < FMath::Square(90.f)) PatrolIndex = (PatrolIndex+1)%PatrolPoints.Num();
 if (AAIController* AI = UnitAI(this)) if (AI->GetMoveStatus() == EPathFollowingStatus::Idle) MoveTo(PatrolPoints[PatrolIndex]);
}
void ABGUnit::HearIncident(const FVector& Location, int32 SourceId) {
 if (!Alive() || IsSeated()) return;
 ABGOperation* Op = Operation(); if (!Op) return;
 const ABGUnit* Source = Op->FindUnit(SourceId);
 if (Source == this) return;
 LastKnown = Location; EvidenceSeconds = FMath::Max(EvidenceSeconds, 15.f);
 if (UnitRole == EBGRole::Guard && Source && Source->UnitRole == EBGRole::Guard) return;
 if (UnitRole == EBGRole::Guard && Source && Op->CanSee(this, Source)) { ThreatId = SourceId; bHostile = true; }
 if (UnitRole == EBGRole::Civilian || (UnitRole == EBGRole::Specialist && !Op->bSpecialistAcquired)) {
  if (AAIController* AI = UnitAI(this)) AI->StopMovement();
 }
}
void ABGUnit::FireAt(ABGUnit* Target) { FireAtEntity(Target); }
void ABGUnit::FireAtEntity(AActor* Target) {
 ABGOperation* Op = Operation(); const auto* Def = WeaponDefinition();
 ABGUnit* TargetUnit = Cast<ABGUnit>(Target);
 ABGVehicle* TargetVehicle = Cast<ABGVehicle>(Target);
 ABGDoor* TargetDoor = Cast<ABGDoor>(Target);
 if (!Op || !Def || !Alive() || !Target || IsSeated() || UGameplayStatics::IsGamePaused(this) ||
  (TargetUnit && (!TargetUnit->Alive() || TargetUnit->IsSeated())) || (TargetVehicle && TargetVehicle->Health <= 0) ||
  (TargetDoor && TargetDoor->Health <= 0) || ShotCooldown > 0 || ReloadRemaining > 0 || !Inventory.IsValidIndex(WeaponIndex)) return;
 FBGItem& Item = Inventory[WeaponIndex];
 if (Item.Ammo <= 0) {
  if (Item.Reserve <= 0) { ShotCooldown = 1.f; if (UnitRole == EBGRole::Operative && bSelection) Op->Notify(TEXT("Weapon empty; no reserve ammunition.")); }
  else Reload();
  return;
 }
 FVector Start = GetActorLocation()+FVector(0,0,18)+GetActorForwardVector()*38;
 const FVector Aim = Target->GetActorLocation()+FVector(0,0,TargetUnit ? 8.f : 0.f);
 if (FVector::Dist(Start, Aim) > Def->Range) return;
 FCollisionQueryParams Params(SCENE_QUERY_STAT(BlackglassShot), false, this);
 FHitResult Sight;
 if (GetWorld()->LineTraceSingleByChannel(Sight, Start, Aim, ECC_Visibility, Params) && Sight.GetActor() != Target) {
  if (!Op->bFriendlyFire) if (const auto* Other = Cast<ABGUnit>(Sight.GetActor())) {
   const bool OpposedBlocker = (UnitRole == EBGRole::Operative && Other->UnitRole == EBGRole::Guard) ||
    (UnitRole == EBGRole::Guard && Other->UnitRole == EBGRole::Operative);
   if (!OpposedBlocker) { Op->Notify(TEXT("Shot blocked by friendly-fire protection.")); return; }
  }
  if (!Cast<ABGUnit>(Sight.GetActor()) && !Cast<ABGVehicle>(Sight.GetActor()) && !Cast<ABGDoor>(Sight.GetActor())) {
   if (UnitRole == EBGRole::Operative) Op->Notify(TEXT("Shot blocked by geometry."));
   return;
  }
 }
 const bool Opposed = TargetUnit && ((UnitRole == EBGRole::Operative && TargetUnit->UnitRole == EBGRole::Guard) ||
  (UnitRole == EBGRole::Guard && TargetUnit->UnitRole == EBGRole::Operative));
 if (TargetUnit && !Op->bFriendlyFire && !Opposed) { Op->Notify(TEXT("Friendly-fire protection blocks this target.")); return; }
 --Item.Ammo; ShotCooldown += Def->Interval; Recoil = 1;
 bHolstered = false;
 FVector Direction = (Aim-Start).GetSafeNormal();
 Direction = Op->Random.VRandCone(Direction, FMath::DegreesToRadians(Def->SpreadDegrees));
 const FVector End = Start+Direction*Def->Range;
 FHitResult Hit;
 const bool Blocked = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
 if (Blocked) {
  if (ABGUnit* Unit = Cast<ABGUnit>(Hit.GetActor())) {
   const bool HostileHit = (UnitRole == EBGRole::Operative && Unit->UnitRole == EBGRole::Guard) ||
    (UnitRole == EBGRole::Guard && Unit->UnitRole == EBGRole::Operative);
   if (Op->bFriendlyFire || HostileHit) Unit->ReceiveHit(Def->Damage, this);
  } else if (ABGVehicle* Car = Cast<ABGVehicle>(Hit.GetActor())) Car->ReceiveHit(Def->Damage);
  else if (ABGDoor* Door = Cast<ABGDoor>(Hit.GetActor())) Door->ReceiveHit(Def->Damage);
  DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 8, FColor(180,160,100), false, .18f);
 }
 DrawDebugLine(GetWorld(), Start, Blocked ? Hit.ImpactPoint : End, Op->bReducedFlash ? FColor(130,130,100) : FColor(220,180,75), false, .055f, 0, Op->bReducedFlash ? .5f : 1.4f);
 Op->ReportIncident(GetActorLocation(), EntityId, Def->HearingRadius);
}
void ABGUnit::ReceiveHit(float Amount, ABGUnit* Source) {
 if (!Alive() || !FMath::IsFinite(Amount) || Amount <= 0) return;
 const float Armor = UnitRole == EBGRole::Operative ? 4.f : UnitRole == EBGRole::Guard ? 2.f : 0.f;
 Health = FMath::Max(0.f, Health-FMath::Max(1.f,Amount-Armor));
 if (Source) { LastKnown = Source->EffectiveLocation(); ThreatId = Source->EntityId; EvidenceSeconds = 25; bHostile = true; }
 if (!Alive()) {
  StopOrders(); GetCharacterMovement()->DisableMovement();
  GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  for (UStaticMeshComponent* Part : Parts) if (Part != SelectionRing) Part->SetRelativeRotation(FRotator(0,0,80));
  if (auto* Op = Operation()) {
   Op->Notify(Label+TEXT(" lost. Carried equipment remains with the casualty."));
   Op->EvaluateMission();
  }
 } else {
  Recoil = .6f;
 }
 UpdateAppearance();
}
FBGUnitRecord ABGUnit::Record() const {
 FBGUnitRecord Data;
 Data.Id=EntityId; Data.Role=UnitRole; Data.Label=Label; Data.Transform=GetActorTransform(); Data.Health=Health;
 Data.Inventory=Inventory; Data.WeaponIndex=WeaponIndex; Data.Holstered=bHolstered; Data.Hold=bHold; Data.Hostile=bHostile;
 Data.HasOrder=bHasOrder; Data.Order=Order; Data.Queue=OrderQueue; Data.ThreatId=ThreatId; Data.LastKnown=LastKnown;
 Data.Evidence=EvidenceSeconds; Data.Cooldown=FMath::Max(0.f,ShotCooldown); Data.ReloadRemaining=ReloadRemaining;
 Data.VehicleId=VehicleId; Data.Seat=SeatIndex; Data.PatrolIndex=PatrolIndex; Data.PatrolPoints=PatrolPoints; return Data;
}
void ABGUnit::RestoreRecord(const FBGUnitRecord& Data) {
 StopOrders(); EntityId=Data.Id; UnitRole=Data.Role; Label=Data.Label; Health=Data.Health; SetActorTransform(Data.Transform);
 Inventory=Data.Inventory; WeaponIndex=Data.WeaponIndex; bHolstered=Data.Holstered; bHold=Data.Hold; bHostile=Data.Hostile;
 bHasOrder=Data.HasOrder; Order=Data.Order; OrderQueue=Data.Queue; ThreatId=Data.ThreatId; LastKnown=Data.LastKnown;
 EvidenceSeconds=Data.Evidence; ShotCooldown=Data.Cooldown; ReloadRemaining=Data.ReloadRemaining;
 VehicleId=Data.VehicleId; SeatIndex=Data.Seat; PatrolIndex=Data.PatrolIndex; PatrolPoints=Data.PatrolPoints; ThinkAccumulator=0;
 DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
 SetActorHiddenInGame(IsSeated());
 GetCapsuleComponent()->SetCollisionEnabled(Alive() && !IsSeated() ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
 if (Alive() && !IsSeated()) GetCharacterMovement()->SetMovementMode(MOVE_Walking); else GetCharacterMovement()->DisableMovement();
 for (UStaticMeshComponent* Part : Parts) if (Part != SelectionRing) Part->SetRelativeRotation(Alive() ? FRotator::ZeroRotator : FRotator(0,0,80));
 UpdateAppearance();
}

ABGVehicle::ABGVehicle() {
 PrimaryActorTick.bCanEverTick = true;
 Hull = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionHull"));
 SetRootComponent(Hull); Hull->SetBoxExtent(FVector(210,95,60));
 Hull->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
 Hull->SetCollisionObjectType(ECC_WorldDynamic); Hull->SetCollisionResponseToAllChannels(ECR_Block);
 Hull->bDynamicObstacle = true;
 Hull->SetAreaClassOverride(UNavArea_Null::StaticClass());
 Hull->SetCanEverAffectNavigation(true);
 UBoxComponent* NavigationObstacle = CreateDefaultSubobject<UBoxComponent>(TEXT("NavigationFootprint"));
 NavigationObstacle->SetupAttachment(Hull);
 NavigationObstacle->SetBoxExtent(FVector(225,110,140));
 NavigationObstacle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 NavigationObstacle->bDynamicObstacle = true;
 NavigationObstacle->SetAreaClassOverride(UNavArea_Null::StaticClass());
 NavigationObstacle->SetCanEverAffectNavigation(true);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
 auto Part = [&](const TCHAR* Name, UStaticMesh* Geometry, FVector Location, FVector Scale) {
  auto* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
  Component->SetupAttachment(Hull); Component->SetStaticMesh(Geometry);
  if (UMaterialInterface* Surface = CorporateSurface()) Component->SetMaterial(0, Surface);
  Component->SetRelativeLocation(Location); Component->SetRelativeScale3D(Scale);
  Component->SetCollisionEnabled(ECollisionEnabled::NoCollision); Component->SetCanEverAffectNavigation(false);
  return Component;
 };
 Part(TEXT("VanBody"),Cube.Object,FVector(0,0,0),FVector(4.1f,1.85f,.9f));
 Part(TEXT("Cabin"),Cube.Object,FVector(-25,0,72),FVector(2.9f,1.73f,.7f));
 Part(TEXT("Windshield"),Cube.Object,FVector(122,0,76),FVector(.08f,1.5f,.48f));
 Part(TEXT("RearWindow"),Cube.Object,FVector(-175,0,72),FVector(.05f,1.5f,.43f));
 for (int32 Index=0; Index<4; ++Index) {
  const FName Name(*FString::Printf(TEXT("Wheel%d"),Index));
  auto* Wheel=CreateDefaultSubobject<UStaticMeshComponent>(Name);
  Wheel->SetupAttachment(Hull); Wheel->SetStaticMesh(Cylinder.Object);
  if (UMaterialInterface* Surface = CorporateSurface()) Wheel->SetMaterial(0, Surface);
  Wheel->SetRelativeLocation(FVector(Index<2 ? 125:-125, Index%2 ? 95:-95,-43));
  Wheel->SetRelativeRotation(FRotator(0,0,90)); Wheel->SetRelativeScale3D(FVector(.65f,.65f,.23f));
  Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision); Wheel->SetCanEverAffectNavigation(false);
 }
 Occupants.Init(0,6);
}
bool ABGVehicle::Board(ABGUnit* Unit) {
 ABGOperation* Op=WorldOperation(this);
 if (UGameplayStatics::IsGamePaused(this)) { if (Op) Op->Notify(TEXT("Boarding waits until tactical pause is released.")); return false; }
 if (!Op || !Unit || !Unit->Alive() || Unit->IsSeated() || Health<=0 || bMoving) {
  if (Op) Op->Notify(TEXT("Boarding requires a living agent and a stationary usable vehicle.")); return false;
 }
 if (FVector::Dist(Unit->GetActorLocation(),GetActorLocation())>330) { Op->Notify(TEXT("Move closer to board the vehicle.")); return false; }
 if (Unit->UnitRole == EBGRole::Specialist && (!Op->bSpecialistAcquired || Unit != Op->Specialist)) return false;
 const int32 Seat=Occupants.IndexOfByKey(0);
 if (Seat==INDEX_NONE) { Op->Notify(TEXT("All six seats are occupied.")); return false; }
 if (Seat==0 && Unit->UnitRole!=EBGRole::Operative) { Op->Notify(TEXT("An operative must take the driver seat first.")); return false; }
 FCollisionQueryParams Params(SCENE_QUERY_STAT(BlackglassBoard),false,Unit); Params.AddIgnoredActor(this);
 FHitResult Hit;
 if (GetWorld()->LineTraceSingleByChannel(Hit, Unit->GetActorLocation(), GetActorLocation()+FVector(0,0,40), ECC_Visibility,Params)) {
  Op->Notify(TEXT("Boarding access is obstructed.")); return false;
 }
 Unit->StopOrders(); Occupants[Seat]=Unit->EntityId; Unit->VehicleId=EntityId; Unit->SeatIndex=Seat;
 Unit->GetCharacterMovement()->DisableMovement(); Unit->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Unit->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale); Unit->SetActorHiddenInGame(true);
 Unit->UpdateAppearance(); return true;
}
bool ABGVehicle::Exit(ABGUnit* Unit) {
 ABGOperation* Op=WorldOperation(this);
 if (UGameplayStatics::IsGamePaused(this)) { if (Op) Op->Notify(TEXT("Disembarking requires releasing tactical pause.")); return false; }
 if (!Op || !Unit || !Unit->Alive() || Unit->VehicleId!=EntityId || !Occupants.IsValidIndex(Unit->SeatIndex)) return false;
 if (bMoving) { Op->Notify(TEXT("Stop the vehicle before disembarking.")); return false; }
 UNavigationSystemV1* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 if (!Nav) { Op->Notify(TEXT("Navigation is unavailable; seat retained.")); return false; }
 const FVector Offsets[]={FVector(0,-190,0),FVector(0,190,0),FVector(-320,0,0),FVector(320,0,0),FVector(-240,-190,0),FVector(-240,190,0)};
 for (const FVector& Offset:Offsets) {
  FNavLocation Projected;
  const FVector Candidate=GetActorLocation()+GetActorRotation().RotateVector(Offset);
  if (!Nav->ProjectPointToNavigation(Candidate,Projected,FVector(70,70,180))) continue;
  const FVector ExitLocation=Projected.Location+FVector(0,0,Unit->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2);
  if (!ClearExit(Unit,ExitLocation,this)) continue;
  FCollisionQueryParams Params(SCENE_QUERY_STAT(BlackglassExitLine),false,Unit); Params.AddIgnoredActor(this);
  FHitResult Hit;
  if (GetWorld()->LineTraceSingleByChannel(Hit,GetActorLocation()+FVector(0,0,45),ExitLocation,ECC_Visibility,Params)) continue;
  Occupants[Unit->SeatIndex]=0; Unit->VehicleId=0; Unit->SeatIndex=INDEX_NONE;
  Unit->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform); Unit->SetActorLocation(ExitLocation);
  Unit->SetActorHiddenInGame(false); Unit->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
  Unit->GetCharacterMovement()->SetMovementMode(MOVE_Walking); Unit->UpdateAppearance();
  if (Occupants[0]==0) StopTravel(); return true;
 }
 Op->Notify(TEXT("All safe exits are blocked. Occupancy is retained.")); return false;
}
bool ABGVehicle::TravelTo(const FVector& Destination) {
 ABGOperation* Op=WorldOperation(this);
 ABGUnit* Driver=Op && Occupants.IsValidIndex(0) ? Op->FindUnit(Occupants[0]):nullptr;
 if (!Op || Health<=0 || !Driver || !Driver->Alive() || Driver->UnitRole!=EBGRole::Operative) {
  if (Op) Op->Notify(TEXT("A living operative in the driver seat is required.")); return false;
 }
 UNavigationSystemV1* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(BlackglassVehicleGround), false, this);
 for (ABGUnit* Person : Op->Units) if (Person) GroundParams.AddIgnoredActor(Person);
 FHitResult StartSurface, GoalSurface;
 const bool HasStart = GetWorld()->LineTraceSingleByChannel(StartSurface,
  GetActorLocation()+FVector(0,0,50), GetActorLocation()-FVector(0,0,500), ECC_Visibility, GroundParams);
 const bool HasGoal = GetWorld()->LineTraceSingleByChannel(GoalSurface,
  Destination+FVector(0,0,200), Destination-FVector(0,0,500), ECC_Visibility, GroundParams);
 FNavLocation Start,Goal;
 if (!Nav || !HasStart || !HasGoal ||
  !Nav->ProjectPointToNavigation(StartSurface.ImpactPoint+FVector(0,0,5),Start,FVector(340,340,35)) ||
  !Nav->ProjectPointToNavigation(GoalSurface.ImpactPoint+FVector(0,0,5),Goal,FVector(150,150,35))) {
  Op->Notify(TEXT("Vehicle destination is outside accessible street navigation.")); return false;
 }
 UNavigationPath* Route=UNavigationSystemV1::FindPathToLocationSynchronously(this,Start.Location,Goal.Location);
 if (!Route || !Route->IsValid() || Route->IsPartial() || Route->PathPoints.Num()<2) {
  UE_LOG(LogTemp, Warning, TEXT("BLACKGLASS vehicle route unavailable: street start=%s goal=%s partial=%d points=%d"),
   *Start.Location.ToString(), *Goal.Location.ToString(), Route && Route->IsPartial(), Route ? Route->PathPoints.Num() : 0);
  Op->Notify(TEXT("No complete vehicle street route.")); return false;
 }
 for (const FVector& Point : Route->PathPoints) if (FMath::Abs(Point.Z - Start.Location.Z) > 80) {
  Op->Notify(TEXT("This vehicle uses street routes; elevated pedestrian paths are unavailable.")); return false;
 }
 Path=Route->PathPoints; PathIndex=1; bMoving=true; return true;
}
void ABGVehicle::StopTravel() { bMoving=false; Path.Empty(); PathIndex=0; }
void ABGVehicle::Tick(float DeltaSeconds) {
 Super::Tick(DeltaSeconds);
 if (!ActorHasTag(TEXT("BGVehicleColored"))) {
  TArray<UStaticMeshComponent*> Components; GetComponents(Components);
  for (UStaticMeshComponent* Part : Components) {
   const FString Name = Part->GetName();
   ColorPart(Part, Name.Contains(TEXT("Window")) || Name.Contains(TEXT("Windshield")) ? FLinearColor(.035f,.075f,.09f) :
    Name.Contains(TEXT("Wheel")) ? FLinearColor(.02f,.02f,.025f) : FLinearColor(.12f,.15f,.17f));
  }
  Tags.Add(TEXT("BGVehicleColored"));
 }
 ABGOperation* Op=WorldOperation(this);
 if (!Op || Op->bRestoring || Op->Outcome!=EBGOutcome::Active || !bMoving) return;
 ABGUnit* Driver=Occupants.IsValidIndex(0) ? Op->FindUnit(Occupants[0]):nullptr;
 if (Health<=0 || !Driver || !Driver->Alive()) { StopTravel(); return; }
 if (!Path.IsValidIndex(PathIndex)) { StopTravel(); return; }
 FVector Goal=Path[PathIndex]; Goal.Z=GetActorLocation().Z;
 const FVector Delta=Goal-GetActorLocation(); const float Length=Delta.Size2D();
 if (Length<35) { ++PathIndex; if (!Path.IsValidIndex(PathIndex)) StopTravel(); return; }
 const FVector Direction=Delta.GetSafeNormal2D();
 const FRotator Facing(0,Direction.Rotation().Yaw,0);
 SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(),Facing,DeltaSeconds,100));
 FHitResult Hit;
 AddActorWorldOffset(Direction*FMath::Min(Length,Speed*DeltaSeconds),true,&Hit);
 if (Hit.bBlockingHit) {
  const AActor* Obstacle = Hit.GetActor();
  const ABGUnit* BlockingUnit = Cast<ABGUnit>(Obstacle);
  UE_LOG(LogTemp, Log, TEXT("BLACKGLASS vehicle blocked: actor=%s class=%s unit=%d vehicle=%s impact=%s"),
   Obstacle ? *Obstacle->GetName() : TEXT("none"), Obstacle ? *Obstacle->GetClass()->GetName() : TEXT("none"),
   BlockingUnit ? BlockingUnit->EntityId : 0, *GetActorLocation().ToString(), *Hit.ImpactPoint.ToString());
  StopTravel(); Op->Notify(TEXT("Vehicle route blocked by an obstacle. Choose a clear street route."));
 }
}
void ABGVehicle::ReceiveHit(float Amount) {
 if (Health<=0 || !FMath::IsFinite(Amount) || Amount<=0) return;
 Health=FMath::Max(0.f,Health-Amount);
 if (Health>0) return;
 StopTravel();
 ABGOperation* Op=WorldOperation(this); if (!Op) return;
 const TArray<int32> Seats=Occupants;
 for (int32 Id:Seats) if (ABGUnit* Unit=Op->FindUnit(Id)) {
  Unit->ReceiveHit(65,nullptr);
  if (Unit->Alive() && !Exit(Unit)) Op->Notify(TEXT("Survivor trapped in the wreck; clear an exit."));
 }
 Op->Notify(TEXT("Vehicle destroyed. Occupants and blocked exits remain tracked."));
 Op->EvaluateMission();
}
bool ABGVehicle::HasSelectedDriver(const TArray<TObjectPtr<ABGUnit>>& Selected) const {
 if (!Occupants.IsValidIndex(0) || !Occupants[0] || Health<=0) return false;
 for (ABGUnit* Unit:Selected) if (Unit && Unit->Alive() && Unit->EntityId==Occupants[0] && Unit->VehicleId==EntityId) return true;
 return false;
}
FBGVehicleRecord ABGVehicle::Record() const {
 FBGVehicleRecord Data; Data.Id=EntityId; Data.Transform=GetActorTransform(); Data.Health=Health; Data.Occupants=Occupants;
 Data.Path=Path; Data.PathIndex=PathIndex; Data.Moving=bMoving; return Data;
}
void ABGVehicle::RestoreRecord(const FBGVehicleRecord& Data) {
 StopTravel(); EntityId=Data.Id; Health=Data.Health; SetActorTransform(Data.Transform);
 Occupants=Data.Occupants; Path=Data.Path; PathIndex=Data.PathIndex; bMoving=Data.Moving && Health>0;
}
void ABGVehicle::RestoreSeats() {
 ABGOperation* Op=WorldOperation(this); if (!Op) return;
 for (int32 Seat=0; Seat<Occupants.Num(); ++Seat) if (ABGUnit* Unit=Op->FindUnit(Occupants[Seat])) {
  Unit->VehicleId=EntityId; Unit->SeatIndex=Seat;
  Unit->GetCharacterMovement()->DisableMovement(); Unit->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Unit->AttachToActor(this,FAttachmentTransformRules::SnapToTargetNotIncludingScale); Unit->SetActorHiddenInGame(true);
  Unit->UpdateAppearance();
 }
}

ABGDoor::ABGDoor() {
 PrimaryActorTick.bCanEverTick=false;
 Leaf=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateLeaf")); SetRootComponent(Leaf);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 Leaf->SetStaticMesh(Cube.Object); Leaf->SetRelativeScale3D(FVector(3.3f,.5f,2.5f));
 if (UMaterialInterface* Surface = CorporateSurface()) Leaf->SetMaterial(0, Surface);
 Leaf->SetCollisionProfileName(TEXT("BlockAllDynamic")); Leaf->SetCanEverAffectNavigation(true);
}
void ABGDoor::Toggle() { if (Health>0) SetOpen(!bOpen); }
void ABGDoor::SetOpen(bool Open) {
 bOpen=Open || Health<=0;
 Leaf->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
 Leaf->SetVisibility(!bOpen);
}
void ABGDoor::ReceiveHit(float Amount) {
 if (Health<=0 || !FMath::IsFinite(Amount) || Amount<=0) return;
 Health=FMath::Max(0.f,Health-Amount);
 if (Health<=0) { SetOpen(true); if (auto* Op=WorldOperation(this)) Op->Notify(TEXT("Barrier destroyed; route and sightline opened.")); }
}
