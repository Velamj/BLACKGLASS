#include "BlackglassGame.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Misc/App.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace {
struct FCameraPresentation {
 FVector Velocity = FVector::ZeroVector;
 float Width = 3300;
 float Yaw = -45;
};
TMap<TWeakObjectPtr<ABGCamera>, FCameraPresentation> CameraPresentation;
FCameraPresentation& Presentation(ABGCamera* CameraPawn) {
 const TWeakObjectPtr<ABGCamera> Key(CameraPawn);
 if (!CameraPresentation.Contains(Key)) {
  FCameraPresentation State;
  State.Width = CameraPawn->Camera->OrthoWidth;
  State.Yaw = CameraPawn->Camera->GetRelativeRotation().Yaw;
  CameraPresentation.Add(Key, State);
 }
 return CameraPresentation.FindChecked(Key);
}
struct FHUDRectangle {
 float X, Y, Width, Height;
 bool Contains(const FVector2D& P) const {
  return P.X >= X && P.Y >= Y && P.X < X + Width && P.Y < Y + Height;
 }
};
struct FHUDLayout {
 float Scale = 1;
 FHUDRectangle Briefing;
 FHUDRectangle Notice;
 FHUDRectangle Controls;
 FHUDRectangle Map;
 TArray<FHUDRectangle> Panels;
};
FHUDLayout Layout(const APlayerController* Controller) {
 int32 Width = 1920, Height = 1080;
 if (Controller) Controller->GetViewportSize(Width, Height);
 FHUDLayout L;
 L.Scale = FMath::Clamp(FMath::Min(Width / 1920.f, Height / 1080.f), .45f, 1.75f);
 const float S = L.Scale;
 L.Briefing = {18*S, 16*S, 660*S, 140*S};
 L.Notice = {18*S, 168*S, 660*S, 38*S};
 L.Controls = {18*S, Height - 190*S, 1000*S, 48*S};
 L.Map = {Width - 266*S, Height - 256*S, 248*S, 238*S};
 for (int32 Index = 0; Index < 4; ++Index)
  L.Panels.Add({18*S + Index*238*S, Height - 124*S, 224*S, 106*S});
 return L;
}
bool OverHUD(const FHUDLayout& L, const FVector2D& P) {
 if (L.Briefing.Contains(P) || L.Notice.Contains(P) ||
     L.Controls.Contains(P) || L.Map.Contains(P)) return true;
 for (const auto& Panel : L.Panels) if (Panel.Contains(P)) return true;
 return false;
}
constexpr float MapMinX = -6000, MapMaxX = 6000;
constexpr float MapMinY = -4200, MapMaxY = 4200;
FHUDRectangle MapInterior(const FHUDLayout& L) {
 return {L.Map.X + 12*L.Scale, L.Map.Y + 38*L.Scale,
         L.Map.Width - 24*L.Scale, L.Map.Height - 52*L.Scale};
}
FVector2D ToMap(const FVector& P, const FHUDRectangle& R) {
 return FVector2D(R.X + (P.X-MapMinX)/(MapMaxX-MapMinX)*R.Width,
                  R.Y + (MapMaxY-P.Y)/(MapMaxY-MapMinY)*R.Height);
}
bool ShiftDown(const APlayerController* PC) {
 return PC && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift));
}
TArray<ABGUnit*> Squad(const ABGOperation* Op) {
 TArray<ABGUnit*> Result;
 if (Op) for (ABGUnit* Unit : Op->Units)
  if (IsValid(Unit) && Unit->UnitRole == EBGRole::Operative) Result.Add(Unit);
 Result.Sort([](const ABGUnit& A, const ABGUnit& B) { return A.EntityId < B.EntityId; });
 return Result;
}
FString Status(const ABGUnit* Unit) {
 if (!Unit->Alive()) return TEXT("DEAD");
 if (Unit->IsSeated()) return TEXT("ABOARD VEHICLE");
 if (Unit->ReloadRemaining > 0) return TEXT("RELOADING");
 if (Unit->bHold) return TEXT("HOLDING POSITION");
 if (Unit->bHasOrder && Unit->Order.Type == EBGOrderType::Attack) return TEXT("ENGAGING");
 if (Unit->bHasOrder) return TEXT("EXECUTING ORDER");
 return Unit->bHolstered ? TEXT("HOLSTERED") : TEXT("WEAPON DRAWN");
}
}

ABGCamera::ABGCamera() {
 RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("CameraOrigin"));
 Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("IsometricCamera"));
 Camera->SetupAttachment(RootComponent);
 Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
 Camera->OrthoWidth = 3300;
 Camera->bConstrainAspectRatio = false;
 Camera->bAutoCalculateOrthoPlanes = true;
 Camera->bUpdateOrthoPlanes = true;
 Camera->bUseCameraHeightAsViewTarget = true;
 const FRotator View(-35.26439f, -45, 0);
 Camera->SetRelativeRotation(View);
 Camera->SetRelativeLocation(-View.Vector()*5500);
 if (UMaterialInterface* Outline = LoadObject<UMaterialInterface>(nullptr,
     TEXT("/Game/Materials/M_BGOperativeOutline.M_BGOperativeOutline"))) {
  Camera->PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, Outline));
  Camera->PostProcessBlendWeight = 1.f;
 }
 SetActorEnableCollision(false);
}
void ABGCamera::Zoom(float Steps) {
 auto& State = Presentation(this);
 State.Width = FMath::Clamp(State.Width*FMath::Pow(.86f, Steps), 1800.f, 8500.f);
}

ABGCommander::ABGCommander() {
 bShowMouseCursor = true;
 bEnableClickEvents = true;
 bEnableMouseOverEvents = false;
 bShouldPerformFullTickWhenPaused = true;
 PrimaryActorTick.bTickEvenWhenPaused = true;
 DefaultMouseCursor = EMouseCursor::Default;
}
void ABGCommander::BeginPlay() {
 Super::BeginPlay();
 if (!GetWorld()) return;
 FTimerDelegate Ready;
 Ready.BindWeakLambda(this, [this]() {
  SelectAll();
  Recenter();
 });
 GetWorld()->GetTimerManager().SetTimerForNextTick(Ready);
}

ABGOperation* ABGCommander::Operation() const {
 return Cast<ABGOperation>(UGameplayStatics::GetGameMode(this));
}
void ABGCommander::SetupInputComponent() {
 Super::SetupInputComponent();
 auto Bind = [this](FKey Key, EInputEvent Event, void (ABGCommander::*Method)()) {
  auto& Binding = InputComponent->BindKey(Key, Event, this, Method);
  Binding.bExecuteWhenPaused = true;
 };
 Bind(EKeys::LeftMouseButton, IE_Pressed, &ABGCommander::LeftDown);
 Bind(EKeys::LeftMouseButton, IE_Released, &ABGCommander::LeftUp);
 Bind(EKeys::RightMouseButton, IE_Pressed, &ABGCommander::ContextOrder);
 Bind(EKeys::One, IE_Pressed, &ABGCommander::SelectOne);
 Bind(EKeys::Two, IE_Pressed, &ABGCommander::SelectTwo);
 Bind(EKeys::Three, IE_Pressed, &ABGCommander::SelectThree);
 Bind(EKeys::Four, IE_Pressed, &ABGCommander::SelectFour);
 Bind(EKeys::SpaceBar, IE_Pressed, &ABGCommander::SelectAll);
 Bind(EKeys::X, IE_Pressed, &ABGCommander::Stop);
 Bind(EKeys::B, IE_Pressed, &ABGCommander::Hold);
 Bind(EKeys::V, IE_Pressed, &ABGCommander::SwitchWeapon);
 Bind(EKeys::R, IE_Pressed, &ABGCommander::Reload);
 Bind(EKeys::H, IE_Pressed, &ABGCommander::Holster);
 Bind(EKeys::E, IE_Pressed, &ABGCommander::BoardOrExit);
 Bind(EKeys::P, IE_Pressed, &ABGCommander::PauseTactical);
 Bind(EKeys::F5, IE_Pressed, &ABGCommander::SaveQuick);
 Bind(EKeys::F9, IE_Pressed, &ABGCommander::LoadQuick);
 Bind(EKeys::F8, IE_Pressed, &ABGCommander::Restart);
 Bind(EKeys::F7, IE_Pressed, &ABGCommander::Abort);
 Bind(EKeys::F, IE_Pressed, &ABGCommander::Recenter);
 Bind(EKeys::MouseScrollUp, IE_Pressed, &ABGCommander::ZoomIn);
 Bind(EKeys::MouseScrollDown, IE_Pressed, &ABGCommander::ZoomOut);
 Bind(EKeys::Q, IE_Pressed, &ABGCommander::RotateCamera);
 FInputKeyBinding Tracking(FInputChord(EKeys::T), IE_Pressed);
 Tracking.bExecuteWhenPaused = true;
 Tracking.KeyDelegate.GetDelegateForManualSet().BindLambda([this]() {
  bTracking = !bTracking;
  if (auto* Op = Operation()) Op->Notify(bTracking ? TEXT("Camera tracking enabled.") : TEXT("Camera tracking disabled."));
 });
 InputComponent->KeyBindings.Add(MoveTemp(Tracking));
 FInputModeGameAndUI Mode;
 Mode.SetHideCursorDuringCapture(false);
 Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
 SetInputMode(Mode);
}
void ABGCommander::PlayerTick(float DeltaSeconds) {
 Super::PlayerTick(DeltaSeconds);
 Selected.RemoveAll([](const TObjectPtr<ABGUnit>& Unit) { return !IsValid(Unit) || !Unit->Alive(); });
 // Only player-owned living bodies enter the visibility pass. NPC detection stays authoritative.
 if (ABGOperation* Op = Operation()) for (ABGUnit* Unit : Op->Units) if (IsValid(Unit)) {
  const bool Owned = Unit->UnitRole == EBGRole::Operative && Unit->Alive() && !Unit->IsSeated();
  for (UStaticMeshComponent* Part : Unit->Parts) if (IsValid(Part)) {
   Part->SetRenderCustomDepth(Owned && Part != Unit->SelectionRing && Part->IsVisible());
   if (Owned) Part->SetCustomDepthStencilValue(Selected.Contains(Unit) ? 2 : 1);
  }
 }
 ABGCamera* Rig = Cast<ABGCamera>(GetPawn());
 if (!Rig || !Rig->Camera) return;
 const float Dt = FMath::Clamp(DeltaSeconds > 0 ? DeltaSeconds : FApp::GetDeltaTime(), 0.f, .1f);
 auto& State = Presentation(Rig);
 float MouseX = 0, MouseY = 0;
 const bool HasMouse = GetMousePosition(MouseX, MouseY);
 if (bDragging && HasMouse) DragEnd = FVector2D(MouseX, MouseY);
 FVector2D Input(IsInputKeyDown(EKeys::D) - IsInputKeyDown(EKeys::A),
                 IsInputKeyDown(EKeys::W) - IsInputKeyDown(EKeys::S));
 int32 Width = 0, Height = 0;
 GetViewportSize(Width, Height);
 if (HasMouse && !bDragging && MouseX >= 0 && MouseX < Width && MouseY >= 0 && MouseY < Height) {
  const float Edge = 18*Layout(this).Scale;
  if (MouseX < Edge) Input.X -= 1-MouseX/Edge;
  else if (MouseX > Width-Edge) Input.X += (MouseX-(Width-Edge))/Edge;
  if (MouseY < Edge) Input.Y += 1-MouseY/Edge;
  else if (MouseY > Height-Edge) Input.Y -= (MouseY-(Height-Edge))/Edge;
 }
 if (!Input.IsNearlyZero()) bTracking = false;
 Input = Input.GetClampedToMaxSize(1);
 const float Yaw = Rig->Camera->GetRelativeRotation().Yaw;
 const FVector Forward = FRotator(0, Yaw, 0).Vector();
 const FVector Right = FRotator(0, Yaw+90, 0).Vector();
 const FVector Desired = (Forward*Input.Y + Right*Input.X)*State.Width*.65f;
 State.Velocity = FMath::VInterpTo(State.Velocity, Desired, Dt, 9);
 FVector Position = Rig->GetActorLocation() + State.Velocity*Dt;
 if (bTracking && !Selected.IsEmpty()) {
  FVector Center = FVector::ZeroVector;
  for (ABGUnit* Unit : Selected) Center += Unit->EffectiveLocation();
  Center /= Selected.Num();
  Position = FMath::VInterpTo(Position, FVector(Center.X, Center.Y, Position.Z), Dt, 5);
 }
 Position.X = FMath::Clamp(Position.X, MapMinX-1200, MapMaxX+1200);
 Position.Y = FMath::Clamp(Position.Y, MapMinY-1200, MapMaxY+1200);
 Rig->SetActorLocation(Position);
 Rig->Camera->OrthoWidth = FMath::FInterpTo(Rig->Camera->OrthoWidth, State.Width, Dt, 10);
 const FRotator View = FMath::RInterpTo(Rig->Camera->GetRelativeRotation(), FRotator(-35.26439f, State.Yaw, 0), Dt, 10);
 Rig->Camera->SetRelativeRotation(View);
 Rig->Camera->SetRelativeLocation(-View.Vector()*5500);
}
void ABGCommander::ApplySelection(const TArray<ABGUnit*>& Units, bool Append) {
 if (!Append) {
  for (ABGUnit* Unit : Selected) if (IsValid(Unit)) Unit->SetSelected(false);
  Selected.Reset();
 }
 for (ABGUnit* Unit : Units) if (IsValid(Unit) && Unit->Alive() && Unit->UnitRole == EBGRole::Operative) {
  Selected.AddUnique(Unit);
  Unit->SetSelected(true);
 }
}
void ABGCommander::LeftDown() {
 bDragging = false;
 float X = 0, Y = 0;
 if (!GetMousePosition(X, Y)) return;
 if (auto* HUD = Cast<ABGHUD>(GetHUD())) if (HUD->ConsumeClick(FVector2D(X,Y))) return;
 DragStart = DragEnd = FVector2D(X,Y);
 bDragging = true;
}
void ABGCommander::LeftUp() {
 if (!bDragging) return;
 bDragging = false;
 float X = 0, Y = 0;
 if (GetMousePosition(X,Y)) DragEnd = FVector2D(X,Y);
 TArray<ABGUnit*> Result;
 if (FVector2D::Distance(DragStart, DragEnd) < 7*Layout(this).Scale) {
  FHitResult Hit;
  GetHitResultUnderCursor(ECC_Visibility, false, Hit);
  ABGUnit* Unit = Cast<ABGUnit>(Hit.GetActor());
  if (Unit && Unit->UnitRole == EBGRole::Operative) Result.Add(Unit);
  else {
   float Closest = 30*Layout(this).Scale;
   for (ABGUnit* Candidate : Squad(Operation())) {
    if (!Candidate->Alive() || Candidate->IsSeated()) continue;
    FVector2D Head, Feet;
    const FVector Location = Candidate->EffectiveLocation();
    if (!ProjectWorldLocationToScreen(Location+FVector(0,0,85), Head) ||
        !ProjectWorldLocationToScreen(Location-FVector(0,0,78), Feet)) continue;
    const FVector2D Segment = Feet-Head;
    const double LengthSquared = Segment.SizeSquared();
    const double Along = LengthSquared > UE_SMALL_NUMBER
        ? FMath::Clamp(FVector2D::DotProduct(DragEnd-Head, Segment)/LengthSquared, 0.0, 1.0) : 0.0;
    const float Distance = FVector2D::Distance(Head+Segment*Along, DragEnd);
    if (Distance < Closest) { Closest = Distance; Unit = Candidate; }
   }
   if (Unit && Unit->UnitRole == EBGRole::Operative) Result.Add(Unit);
  }
 } else {
  const FVector2D Min(FMath::Min(DragStart.X,DragEnd.X), FMath::Min(DragStart.Y,DragEnd.Y));
  const FVector2D Max(FMath::Max(DragStart.X,DragEnd.X), FMath::Max(DragStart.Y,DragEnd.Y));
  for (ABGUnit* Unit : Squad(Operation())) {
   FVector2D Screen;
   if (Unit->Alive() && ProjectWorldLocationToScreen(Unit->EffectiveLocation()+FVector(0,0,70), Screen) &&
       Screen.X >= Min.X && Screen.X <= Max.X && Screen.Y >= Min.Y && Screen.Y <= Max.Y) Result.Add(Unit);
  }
 }
 ApplySelection(Result, ShiftDown(this));
}
void ABGCommander::ContextOrder() {
 auto* Op = Operation();
 if (!Op || Op->Outcome != EBGOutcome::Active) return;
 float X = 0, Y = 0;
 if (!GetMousePosition(X,Y) || OverHUD(Layout(this), FVector2D(X,Y))) return;
 if (Selected.IsEmpty()) { Op->Notify(TEXT("Select an operative before issuing an order.")); return; }
 const bool ForceAttack = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
 FHitResult Hit;
 FVector RayOrigin, RayDirection;
 FCollisionQueryParams Pick(SCENE_QUERY_STAT(BlackglassCommandPick), false);
 for (ABGUnit* Unit : Op->Units)
  if (IsValid(Unit) && Unit->UnitRole != EBGRole::Operative && !Op->IsDetected(Unit)) Pick.AddIgnoredActor(Unit);
 if (!DeprojectMousePositionToWorld(RayOrigin, RayDirection) ||
     !GetWorld()->LineTraceSingleByChannel(Hit, RayOrigin, RayOrigin + RayDirection*50000, ECC_Visibility, Pick)) {
  Op->Notify(ForceAttack ? TEXT("Choose a visible attackable target.") : TEXT("No accessible surface under the cursor.")); return;
 }
 FBGOrder NewOrder;
 NewOrder.Location = Hit.ImpactPoint;
 ABGDoor* DoorTarget = nullptr;
 if (ABGUnit* Target = Cast<ABGUnit>(Hit.GetActor())) {
  if (!Target->Alive()) { Op->Notify(ForceAttack ? TEXT("Choose a visible attackable target.") : TEXT("That target is dead. Choose another target or accessible ground.")); return; }
  if (ForceAttack || Target->UnitRole == EBGRole::Guard) {
   if (!Op->IsDetected(Target)) { Op->Notify(ForceAttack ? TEXT("Choose a visible attackable target.") : TEXT("Choose accessible ground or a visible target.")); return; }
   NewOrder.Type = EBGOrderType::Attack;
   NewOrder.TargetId = Target->EntityId;
  } else if (Target->UnitRole == EBGRole::Specialist) {
   if (!Op->IsDetected(Target)) { Op->Notify(TEXT("Choose accessible ground or a visible interaction.")); return; }
   NewOrder.Type = EBGOrderType::Interact;
   NewOrder.TargetId = Target->EntityId;
  }
 } else if (ABGDoor* Door = Cast<ABGDoor>(Hit.GetActor())) {
  if (Door->Health <= 0) { Op->Notify(ForceAttack ? TEXT("Choose a visible attackable target.") : TEXT("That door has been destroyed.")); return; }
  NewOrder.Type = ForceAttack ? EBGOrderType::Attack : EBGOrderType::Interact;
  NewOrder.TargetId = Door->EntityId;
  DoorTarget = Door;
 } else if (ABGVehicle* Vehicle = Cast<ABGVehicle>(Hit.GetActor())) {
  if (Vehicle->Health <= 0) { Op->Notify(ForceAttack ? TEXT("Choose a visible attackable target.") : TEXT("That vehicle has been destroyed.")); return; }
  NewOrder.Type = ForceAttack ? EBGOrderType::Attack : EBGOrderType::Board;
  NewOrder.TargetId = Vehicle->EntityId;
 }
 if (ForceAttack && NewOrder.Type != EBGOrderType::Attack) {
  Op->Notify(TEXT("Choose a visible attackable target.")); return;
 }
 if (NewOrder.TargetId && !IsValid(Op->FindEntity(NewOrder.TargetId))) {
  Op->Notify(ForceAttack ? TEXT("Choose a visible attackable target.") : TEXT("Target is no longer available.")); return;
 }
 const bool Append = ShiftDown(this);
 if (DoorTarget && NewOrder.Type == EBGOrderType::Interact) {
  ABGUnit* Nearest = nullptr;
  double NearestDistance = TNumericLimits<double>::Max();
  for (ABGUnit* Unit : Selected) if (IsValid(Unit) && Unit->Alive() && !Unit->IsSeated()) {
   const double Distance = FVector::DistSquared(Unit->EffectiveLocation(), DoorTarget->GetActorLocation());
   if (Distance < NearestDistance) { NearestDistance = Distance; Nearest = Unit; }
  }
  if (!Nearest) { Op->Notify(TEXT("Disembark a selected operative before interacting with a door.")); return; }
  Nearest->IssueOrder(NewOrder, Append);
  return;
 }
 if (NewOrder.Type == EBGOrderType::Move && Op->Vehicle && Op->Vehicle->HasSelectedDriver(Selected)) {
  if (!Op->Vehicle->TravelTo(NewOrder.Location)) Op->Notify(TEXT("Vehicle cannot reach that destination."));
  return;
 }
 UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 int32 MovingIndex = 0;
 for (ABGUnit* Unit : Selected) {
  if (!IsValid(Unit) || !Unit->Alive()) continue;
  FBGOrder Order = NewOrder;
  if (Order.Type == EBGOrderType::Attack && Unit->IsSeated()) {
   Op->Notify(TEXT("Disembark to fire an operative's weapon.")); continue;
  }
  if (Order.Type == EBGOrderType::Move) {
   if (Unit->IsSeated()) { Op->Notify(TEXT("Select the driver to command the vehicle, or press E to disembark.")); continue; }
   const float OffsetX = Selected.Num() > 1 ? (MovingIndex%2-.5f)*135 : 0;
   const float OffsetY = Selected.Num() > 1 ? (MovingIndex/2-.5f)*135 : 0;
   FNavLocation Projected;
   if (!Nav || !Nav->ProjectPointToNavigation(Order.Location+FVector(OffsetX,OffsetY,0), Projected, FVector(240,240,500))) {
    Op->Notify(TEXT("Destination is outside accessible navigation.")); continue;
   }
   Order.Location = Projected.Location;
   ++MovingIndex;
  }
  Unit->IssueOrder(Order, Append);
 }
}

void ABGCommander::SelectAll() {
 TArray<ABGUnit*> Units;
 for (ABGUnit* Unit : Squad(Operation())) if (Unit->Alive()) Units.Add(Unit);
 ApplySelection(Units);
}
void ABGCommander::SelectNumber(int32 Index) {
 const auto Units = Squad(Operation());
 if (Units.IsValidIndex(Index)) ApplySelection({Units[Index]}, ShiftDown(this));
}
void ABGCommander::SelectOne() { SelectNumber(0); }
void ABGCommander::SelectTwo() { SelectNumber(1); }
void ABGCommander::SelectThree() { SelectNumber(2); }
void ABGCommander::SelectFour() { SelectNumber(3); }
void ABGCommander::Stop() {
 for (ABGUnit* Unit : Selected) if (IsValid(Unit)) Unit->StopOrders(false);
 if (auto* Op = Operation()) if (Op->Vehicle && Op->Vehicle->HasSelectedDriver(Selected)) Op->Vehicle->StopTravel();
}
void ABGCommander::Hold() {
 for (ABGUnit* Unit : Selected) if (IsValid(Unit)) Unit->StopOrders(true);
 if (auto* Op = Operation()) if (Op->Vehicle && Op->Vehicle->HasSelectedDriver(Selected)) Op->Vehicle->StopTravel();
}
void ABGCommander::SwitchWeapon() { for (ABGUnit* Unit : Selected) if (IsValid(Unit)) Unit->SwitchWeapon(); }
void ABGCommander::Reload() { for (ABGUnit* Unit : Selected) if (IsValid(Unit)) Unit->Reload(); }
void ABGCommander::Holster() {
 if (Selected.IsEmpty()) return;
 const bool Holstered = !Selected[0]->bHolstered;
 for (ABGUnit* Unit : Selected) if (IsValid(Unit)) { Unit->bHolstered = Holstered; Unit->UpdateAppearance(); }
 if (auto* Op = Operation()) Op->Notify(Holstered ? TEXT("Weapons holstered. Witnessed incidents remain recorded.") : TEXT("Weapons drawn."));
}
void ABGCommander::BoardOrExit() {
 auto* Op = Operation();
 if (!Op || !Op->Vehicle || Op->Outcome != EBGOutcome::Active) return;
 for (ABGUnit* Unit : Selected) {
  if (!IsValid(Unit) || !Unit->Alive()) continue;
  if (Unit->IsSeated()) Op->Vehicle->Exit(Unit);
  else {
   FBGOrder Order; Order.Type = EBGOrderType::Board; Order.TargetId = Op->Vehicle->EntityId;
   Order.Location = Op->Vehicle->GetActorLocation(); Unit->IssueOrder(Order, ShiftDown(this));
  }
 }
}
void ABGCommander::PauseTactical() {
 const bool Paused = !UGameplayStatics::IsGamePaused(this);
 UGameplayStatics::SetGamePaused(this, Paused);
 if (auto* Op = Operation()) Op->Notify(Paused ? TEXT("Tactical pause: issue or queue orders; P resumes.") : TEXT("Real-time operation resumed."));
}
void ABGCommander::SaveQuick() { if (auto* Op = Operation()) Op->SaveOperation(TEXT("Quick")); }
void ABGCommander::LoadQuick() {
 if (auto* Op = Operation()) if (Op->LoadOperation(TEXT("Quick")))
  if (ABGCamera* Rig = Cast<ABGCamera>(GetPawn())) CameraPresentation.Remove(TWeakObjectPtr<ABGCamera>(Rig));
}
void ABGCommander::Restart() {
 UGameplayStatics::SetGamePaused(this, false);
 if (auto* Op = Operation()) Op->RestartOperation();
}
void ABGCommander::Abort() {
 if (auto* Op = Operation()) if (Op->Outcome == EBGOutcome::Active) {
  Stop(); Op->Outcome = EBGOutcome::Aborted; Op->Notify(TEXT("Operation aborted. No acquisition reward. F8 restarts."));
 }
}
void ABGCommander::Recenter() {
 if (Selected.IsEmpty()) return;
 if (ABGCamera* Rig = Cast<ABGCamera>(GetPawn())) {
  FVector Center = FVector::ZeroVector;
  for (ABGUnit* Unit : Selected) Center += Unit->EffectiveLocation();
  Center /= Selected.Num(); Center.Z = Rig->GetActorLocation().Z;
  Rig->SetActorLocation(Center); Presentation(Rig).Velocity = FVector::ZeroVector;
 }
}
void ABGCommander::ZoomIn() { if (ABGCamera* Rig = Cast<ABGCamera>(GetPawn())) Rig->Zoom(1); }
void ABGCommander::ZoomOut() { if (ABGCamera* Rig = Cast<ABGCamera>(GetPawn())) Rig->Zoom(-1); }
void ABGCommander::RotateCamera() {
 if (ABGCamera* Rig = Cast<ABGCamera>(GetPawn())) Presentation(Rig).Yaw = FMath::UnwindDegrees(Presentation(Rig).Yaw+90);
}

float ABGHUD::UIScale() const { return Layout(GetOwningPlayerController()).Scale; }
bool ABGHUD::ConsumeClick(const FVector2D& ScreenPoint) {
 ABGCommander* Commander = Cast<ABGCommander>(GetOwningPlayerController());
 if (!Commander) return false;
 const auto L = Layout(Commander);
 for (int32 Index = 0; Index < L.Panels.Num(); ++Index)
  if (L.Panels[Index].Contains(ScreenPoint)) { Commander->SelectNumber(Index); return true; }
 if (L.Map.Contains(ScreenPoint)) {
  const auto Map = MapInterior(L);
  if (Map.Contains(ScreenPoint)) if (ABGCamera* Rig = Cast<ABGCamera>(Commander->GetPawn())) {
   const float X = FMath::Lerp(MapMinX,MapMaxX,(ScreenPoint.X-Map.X)/Map.Width);
   const float Y = FMath::Lerp(MapMaxY,MapMinY,(ScreenPoint.Y-Map.Y)/Map.Height);
   Rig->SetActorLocation(FVector(X,Y,Rig->GetActorLocation().Z));
   Presentation(Rig).Velocity = FVector::ZeroVector;
   Commander->bTracking = false;
  }
  return true;
 }
 return OverHUD(L,ScreenPoint);
}
void ABGHUD::DrawHUD() {
 Super::DrawHUD();
 if (!Canvas) return;
 ABGCommander* Commander = Cast<ABGCommander>(GetOwningPlayerController());
 ABGOperation* Op = Commander ? Commander->Operation() : nullptr;
 if (!Commander || !Op || !GEngine) return;
 const auto L = Layout(Commander);
 const float S = L.Scale;
 const FLinearColor White(.89f,.92f,.91f,1), Muted(.56f,.64f,.62f,1);
 const FLinearColor Accent(.32f,.79f,.67f,1), Red(.88f,.32f,.28f,1);
 auto Box = [&](const FHUDRectangle& R, FLinearColor Color) { DrawRect(Color,R.X,R.Y,R.Width,R.Height); };
 auto Text = [&](const FString& Value, float X, float Y, FLinearColor Color, float Size = 1.f) {
  DrawText(Value,Color,X,Y,GEngine->GetMediumFont(),S*Size,false);
 };
 auto Border = [&](const FHUDRectangle& R, FLinearColor Color) {
  DrawLine(R.X,R.Y,R.X+R.Width,R.Y,Color,S);
  DrawLine(R.X,R.Y+R.Height,R.X+R.Width,R.Y+R.Height,Color,S);
  DrawLine(R.X,R.Y,R.X,R.Y+R.Height,Color,S);
  DrawLine(R.X+R.Width,R.Y,R.X+R.Width,R.Y+R.Height,Color,S);
 };
 Box(L.Briefing,FLinearColor(.023f,.035f,.038f,.94f)); Border(L.Briefing,Muted);
 const float BX = L.Briefing.X+16*S, BY = L.Briefing.Y+12*S;
 Text(TEXT("BLACKGLASS  /  DEPOT BLOCK"),BX,BY,Accent,1.12f);
 FString Objective = Op->bSpecialistAcquired
  ? TEXT("Extract the specialist and surviving squad at the west marker.")
  : TEXT("Acquire the research specialist inside the controlled facility.");
 Text(Objective,BX,BY+27*S,White,.92f);
 Text(TEXT("Right-click orders  |  Ctrl: force attack  |  Shift: queue"),BX,BY+49*S,Muted,.84f);
 int32 LivingOperatives = 0, ExtractedOperatives = 0;
 for (ABGUnit* Unit : Op->Units) if (IsValid(Unit) && Unit->UnitRole == EBGRole::Operative && Unit->Alive()) {
  ++LivingOperatives;
  if (FVector::DistSquared2D(Unit->EffectiveLocation(), Op->ExtractionCenter) <= FMath::Square(Op->ExtractionRadius)) ++ExtractedOperatives;
 }
 const bool SpecialistAtExtraction = IsValid(Op->Specialist) && Op->Specialist->Alive() &&
  FVector::DistSquared2D(Op->Specialist->EffectiveLocation(), Op->ExtractionCenter) <= FMath::Square(Op->ExtractionRadius);
 const FString Progress = Op->bSpecialistAcquired
  ? FString::Printf(TEXT("EXTRACTION %d/%d living operatives | Specialist: %s"),ExtractedOperatives,LivingOperatives,
     SpecialistAtExtraction ? TEXT("AT MARKER") : TEXT("EN ROUTE"))
  : TEXT("Doors provide a service route. Public movement favors holstered weapons.");
 Text(Progress,BX,BY+70*S,Muted,.80f);
 Text(FString::Printf(TEXT("SECURITY: %s    TIME %02d:%02d    CREDITS %d"),
      Op->AlarmSeconds > 0 ? TEXT("LOCAL ALERT") : TEXT("ROUTINE PATROL"),
      int32(Op->SimulationSeconds)/60,int32(Op->SimulationSeconds)%60,Op->Credits),BX,BY+91*S,
      Op->AlarmSeconds > 0 ? Red : White,.9f);
 FString State = UGameplayStatics::IsGamePaused(this) ? TEXT("TACTICAL PAUSE: orders remain available; P resumes.") : TEXT("REAL-TIME OPERATION");
 if (Op->Outcome == EBGOutcome::Success) State = TEXT("ACQUISITION COMPLETE  /  Reward settled  /  F8 replay");
 else if (Op->Outcome == EBGOutcome::Failed) State = TEXT("OPERATION FAILED  /  F8 restart or F9 load");
 else if (Op->Outcome == EBGOutcome::Aborted) State = TEXT("OPERATION ABORTED  /  F8 restart or F9 load");
 Text(State,BX,BY+114*S,Op->Outcome == EBGOutcome::Failed ? Red : Accent,.84f);
 Box(L.Notice,FLinearColor(.035f,.045f,.05f,.94f));
 const FString Notice = !Op->Notice.IsEmpty() && Op->SimulationSeconds <= Op->NoticeExpires
  ? Op->Notice : TEXT("Select operatives with 1-4, drag or Space. Shift queues orders.");
 const float NoticeWidth = L.Notice.Width-28*S;
 TArray<FString> Words, NoticeLines;
 Notice.ParseIntoArrayWS(Words);
 float NoticeScale = .88f*S;
 for (int32 Pass = 0; Pass < 9; ++Pass) {
  NoticeLines.Reset();
  FString Line;
  for (const FString& Word : Words) {
   const FString Candidate = Line.IsEmpty() ? Word : Line+TEXT(" ")+Word;
   float Width = 0, Height = 0;
   GetTextSize(Candidate,Width,Height,GEngine->GetMediumFont(),NoticeScale);
   if (!Line.IsEmpty() && Width > NoticeWidth) {
    NoticeLines.Add(Line); Line = Word;
   } else Line = Candidate;
  }
  if (!Line.IsEmpty()) NoticeLines.Add(Line);
  if (NoticeLines.Num() <= 2) break;
  NoticeScale *= .9f;
 }
 float LineHeight = 0, IgnoredWidth = 0, WidestLine = 0;
 GetTextSize(TEXT("Mg"),IgnoredWidth,LineHeight,GEngine->GetMediumFont(),NoticeScale);
 for (const FString& Line : NoticeLines) {
  float Width = 0, Height = 0;
  GetTextSize(Line,Width,Height,GEngine->GetMediumFont(),NoticeScale);
  WidestLine = FMath::Max(WidestLine,Width);
 }
 const float LineGap = 2*S;
 const float GapHeight = LineGap*FMath::Max(0,NoticeLines.Num()-1);
 const float HeightRoom = FMath::Max(1.f,L.Notice.Height-10*S-GapHeight);
 const float Fit = FMath::Min(1.f,FMath::Min(
  NoticeWidth/FMath::Max(1.f,WidestLine),
  HeightRoom/FMath::Max(1.f,LineHeight*NoticeLines.Num())));
 NoticeScale *= Fit;
 LineHeight *= Fit;
 const float NoticeHeight = LineHeight*NoticeLines.Num()+GapHeight;
 float NoticeY = L.Notice.Y+(L.Notice.Height-NoticeHeight)*.5f;
 for (const FString& Line : NoticeLines) {
  DrawText(Line,White,L.Notice.X+14*S,NoticeY,GEngine->GetMediumFont(),NoticeScale,false);
  NoticeY += LineHeight+LineGap;
 }
 Box(L.Controls,FLinearColor(.02f,.03f,.035f,.94f));
 Text(TEXT("WASD/edges pan  Wheel zoom  Q rotate  F center  T track  H holster  V weapon  R reload"),
      L.Controls.X+10*S,L.Controls.Y+6*S,Muted,.8f);
 Text(TEXT("E board/exit  X stop  B hold  P pause  F5 save  F9 load  F8 restart  F7 abort"),
      L.Controls.X+10*S,L.Controls.Y+28*S,Muted,.8f);
 const auto Units = Squad(Op);
 for (int32 Index = 0; Index < 4; ++Index) {
  const auto& R = L.Panels[Index];
  ABGUnit* Unit = Units.IsValidIndex(Index) ? Units[Index] : nullptr;
  const bool Selected = Unit && Commander->Selected.Contains(Unit);
  Box(R,FLinearColor(.025f,.04f,.043f,.97f)); Border(R,Selected ? Accent : Muted);
  if (!Unit) { Text(TEXT("UNASSIGNED"),R.X+12*S,R.Y+12*S,Muted); continue; }
  Text(FString::Printf(TEXT("%d  %s"),Index+1,*Unit->Label),R.X+12*S,R.Y+10*S,Selected ? Accent : White,.9f);
  DrawRect(FLinearColor(.12f,.16f,.16f,1),R.X+12*S,R.Y+31*S,R.Width-24*S,8*S);
  DrawRect(Unit->Health > 35 ? Accent : Red,R.X+12*S,R.Y+31*S,
           (R.Width-24*S)*FMath::Clamp(Unit->Health/100.f,0.f,1.f),8*S);
  Text(FString::Printf(TEXT("HEALTH %.0f"),FMath::Max(Unit->Health,0.f)),R.X+12*S,R.Y+44*S,White,.78f);
  const auto* Definition = Unit->WeaponDefinition();
  FString Weapon = Definition ? Definition->Label : TEXT("No equipped weapon");
  if (Unit->Inventory.IsValidIndex(Unit->WeaponIndex)) {
   const auto& Item = Unit->Inventory[Unit->WeaponIndex];
   Weapon += FString::Printf(TEXT("  %d / %d"),Item.Ammo,Item.Reserve);
  }
  Text(Weapon.Left(31),R.X+12*S,R.Y+63*S,Muted,.75f);
  Text(Status(Unit),R.X+12*S,R.Y+84*S,Unit->Alive() ? Accent : Red,.75f);
 }
 Box(L.Map,FLinearColor(.025f,.04f,.043f,.97f)); Border(L.Map,Muted);
 Text(TEXT("DISTRICT / CLICK TO PAN"),L.Map.X+12*S,L.Map.Y+10*S,Accent,.76f);
 const auto Map = MapInterior(L);
 Box(Map,FLinearColor(.095f,.12f,.12f,1));
 auto MapRoad = [&](float MinX, float MinY, float MaxX, float MaxY) {
  const auto TopLeft = ToMap(FVector(MinX,MaxY,0),Map);
  const auto BottomRight = ToMap(FVector(MaxX,MinY,0),Map);
  DrawRect(FLinearColor(.20f,.24f,.24f,1),TopLeft.X,TopLeft.Y,
           BottomRight.X-TopLeft.X,BottomRight.Y-TopLeft.Y);
 };
 MapRoad(-2200,-3700,-1200,3700);
 MapRoad(-4700,-2300,4700,-1300);
 MapRoad(-4800,1700,-100,2700);
 const auto Extraction = ToMap(Op->ExtractionCenter,Map);
 DrawRect(Accent,Extraction.X-5*S,Extraction.Y-5*S,10*S,10*S);
 for (ABGUnit* Unit : Op->Units) {
  if (!IsValid(Unit) || !Unit->Alive()) continue;
  const bool Controlled = Unit->UnitRole == EBGRole::Operative;
  if (!Controlled && !Op->IsDetected(Unit)) continue;
  const auto P = ToMap(Unit->EffectiveLocation(),Map);
  if (!Map.Contains(P)) continue;
  const FLinearColor Color = Controlled ? Accent :
   Unit->UnitRole == EBGRole::Guard ? Red : Unit->UnitRole == EBGRole::Specialist ? FLinearColor(.92f,.80f,.37f,1) : Muted;
  DrawRect(Color,P.X-2.5f*S,P.Y-2.5f*S,5*S,5*S);
 }
 if (Op->Vehicle && Op->Vehicle->Health > 0) {
  const auto P = ToMap(Op->Vehicle->GetActorLocation(),Map);
  DrawRect(White,P.X-4*S,P.Y-2*S,8*S,4*S);
 }
 if (ABGCamera* Rig = Cast<ABGCamera>(Commander->GetPawn())) {
  const auto P = ToMap(Rig->GetActorLocation(),Map);
  DrawLine(P.X-5*S,P.Y,P.X+5*S,P.Y,White,S);
  DrawLine(P.X,P.Y-5*S,P.X,P.Y+5*S,White,S);
 }
 for (ABGUnit* Unit : Squad(Op)) {
  FVector2D P;
  if (!IsValid(Unit) || !Unit->Alive() || Unit->IsSeated() ||
      !Commander->ProjectWorldLocationToScreen(Unit->EffectiveLocation()+FVector(0,0,85),P) ||
      OverHUD(L,P)) continue;
  const bool Chosen = Commander->Selected.Contains(Unit);
  const FString Label = FString::Printf(TEXT("%d %s"),Unit->EntityId,*Unit->Label);
  float LabelWidth = 0, LabelHeight = 0;
  GetTextSize(Label,LabelWidth,LabelHeight,GEngine->GetMediumFont(),.67f*S);
  const float LabelX = P.X-LabelWidth*.5f, LabelY = P.Y-18*S;
  const FHUDRectangle Tag{LabelX-4*S,LabelY-2*S,LabelWidth+8*S,LabelHeight+4*S};
  if (OverHUD(L,FVector2D(Tag.X,Tag.Y)) || OverHUD(L,FVector2D(Tag.X+Tag.Width,Tag.Y+Tag.Height))) continue;
  Box(Tag,FLinearColor(.018f,.027f,.03f,.85f));
  DrawLine(P.X,P.Y-3*S,P.X,P.Y+3*S,Chosen ? Accent : Muted,S);
  Text(Label,LabelX,LabelY,Chosen ? Accent : White,.67f);
 }
 if (Commander->bDragging) {
  const auto A = Commander->DragStart, B = Commander->DragEnd;
  const FHUDRectangle R{FMath::Min(A.X,B.X),FMath::Min(A.Y,B.Y),FMath::Abs(A.X-B.X),FMath::Abs(A.Y-B.Y)};
  Box(R,FLinearColor(.15f,.65f,.5f,.12f)); Border(R,Accent);
 }
}
