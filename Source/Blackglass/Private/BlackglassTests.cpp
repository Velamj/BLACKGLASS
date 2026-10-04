#include "BlackglassGame.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "UObject/StrongObjectPtr.h"

namespace {
class FBGFoundationScenario final : public IAutomationLatentCommand {
 FAutomationTestBase* Test;
 int32 Stage = 0;
 double Began = FPlatformTime::Seconds(), StageBegan = Began;
 float StageWorldTime = 0;
 TStrongObjectPtr<UBGSave> Original;
 TStrongObjectPtr<UBGSave> Checkpoint;
 TWeakObjectPtr<UWorld> RestartedWorld;
 TArray<FVector> StartLocations, Goals;
 FVector VehicleStart, BoardingEdgeStart, BoardingClearGoal;
 float BoardingEdgeTravel = 0;
 int32 PausePhase = 0, PausedAmmo = 0, ExtractionPhase = 0, BoardingEdgePhase = 0;
 int32 SelectionOutlinePhase = 0, RestartOutlinePhase = 0;
 float OutlineChangedAt = 0, GroupSeatedAt = -1;
 bool CheckpointOutlinePending = false;
 float PausedHealth = 0, PausedSimulation = 0;
 double PauseBegan = 0;
 bool Failed = false;
 const FString Slot = TEXT("NativeFoundationTest");

 ABGOperation* Operation() const {
  if (!GEngine) return nullptr;
  for (const FWorldContext& Context : GEngine->GetWorldContexts())
   if (UWorld* World = Context.World())
    if (World->IsGameWorld())
     if (auto* Op = Cast<ABGOperation>(World->GetAuthGameMode())) return Op;
  return nullptr;
 }
 bool Check(bool Condition, const FString& Message) {
  Test->TestTrue(Message, Condition);
  Failed |= !Condition;
  return Condition;
 }
 void Next(ABGOperation* Op, const TCHAR* Evidence) {
  Test->AddInfo(Evidence);
  ++Stage; StageBegan = FPlatformTime::Seconds();
  StageWorldTime = Op ? Op->GetWorld()->GetTimeSeconds() : 0;
 }
 bool Expired(ABGOperation* Op, float Limit, const TCHAR* Activity) {
  const float Simulated = Op ? Op->GetWorld()->GetTimeSeconds() - StageWorldTime : 0;
  if (Simulated < Limit && FPlatformTime::Seconds()-StageBegan < Limit*2) return false;
  Test->AddError(FString::Printf(TEXT("Stage %d timed out: %s (%.2fs of world ticks)."), Stage, Activity, Simulated));
  for (int32 Id=1; Op && Id<=4; ++Id)
   if (ABGUnit* Unit=Op->FindUnit(Id))
    Test->AddInfo(FString::Printf(TEXT("Operative %d at %s; order=%d; active=%d; seat=%d"), Id,
     *Unit->GetActorLocation().ToString(), static_cast<int32>(Unit->Order.Type), Unit->bHasOrder, Unit->SeatIndex));
  Failed = true;
  return true;
 }
 void QuietFixtures(ABGOperation* Op) {
  // This isolates runtime contracts from unrelated combat. It is not an AI difficulty setting.
  for (ABGUnit* Unit : Op->Units)
   if (Unit && Unit->UnitRole != EBGRole::Operative && Unit->UnitRole != EBGRole::Specialist) {
    Unit->StopOrders(true); Unit->SetActorTickEnabled(false);
   }
  Op->AlarmSeconds = 0;
 }
 bool Move(ABGOperation* Op, int32 Id, const FVector& Destination, bool Append=false) {
  ABGUnit* Unit=Op->FindUnit(Id);
  auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(Op->GetWorld());
  FNavLocation Projected;
  if (!Check(Unit && Nav && Nav->ProjectPointToNavigation(Destination,Projected,FVector(100,100,500)),
   FString::Printf(TEXT("Reachable navigation destination for operative %d"), Id))) return false;
  FBGOrder Order; Order.Location=Projected.Location;
  Unit->IssueOrder(Order,Append);
  return true;
 }
 bool Near(ABGOperation* Op, int32 Id, const FVector& Destination, float Radius=160) const {
  ABGUnit* Unit=Op->FindUnit(Id);
  return Unit && FVector::Dist2D(Unit->EffectiveLocation(),Destination)<Radius;
 }
 bool Finish(bool Restore) {
  if (Restore && Original.IsValid())
   if (ABGOperation* Op=Operation()) {
    for (ABGUnit* Unit : Op->Units) if (Unit) Unit->SetActorTickEnabled(true);
    Op->RestoreOperation(Original.Get());
   }
  return true;
 }
 bool VerifyOutlineState(ABGOperation* Op, const TArray<int32>& ActiveIds,
  const TArray<int32>& SelectedIds, const TCHAR* State) {
  auto* Commander=Cast<ABGCommander>(UGameplayStatics::GetPlayerController(Op,0));
  bool SelectionMatches=Commander && Commander->Selected.Num()==SelectedIds.Num();
  for(int32 Id : SelectedIds) SelectionMatches &= Commander && Commander->Selected.Contains(Op->FindUnit(Id));
  Check(SelectionMatches,FString(State)+TEXT(": authoritative selection matches the fixture"));
  int32 ActiveUnits=0;
  for(ABGUnit* Unit : Op->Units) if(IsValid(Unit)) {
   const bool ExpectedActive=ActiveIds.Contains(Unit->EntityId);
   bool Matches=!Unit->Parts.IsEmpty(), HasBody=false;
   for(UStaticMeshComponent* Part : Unit->Parts) if(IsValid(Part)) {
    const bool Body=Part!=Unit->SelectionRing && Part->IsVisible();
    if(ExpectedActive && Body) {
     HasBody=true;
     Matches &= Part->bRenderCustomDepth &&
      Part->CustomDepthStencilValue==(SelectedIds.Contains(Unit->EntityId) ? 2 : 1);
    } else Matches &= !Part->bRenderCustomDepth;
   }
   if(ExpectedActive) { ++ActiveUnits; Matches &= HasBody && Unit->UnitRole==EBGRole::Operative; }
   Check(Matches,FString::Printf(TEXT("%s: entity %d has the expected owned-body outline state; marker excluded"),
    State,Unit->EntityId));
  }
  Check(ActiveUnits==ActiveIds.Num(),FString(State)+TEXT(": all expected owned entities exist"));
  return !Failed;
 }
 bool VerifyCheckpoint(ABGOperation* Op) {
  Check(Op->Units.Num()==Checkpoint->Units.Num(),TEXT("Load restores the same entity count"));
  TSet<int32> Ids;
  for (ABGUnit* Unit : Op->Units) if(Unit) Ids.Add(Unit->EntityId);
  Check(Ids.Num()==Op->Units.Num(),TEXT("Load creates no duplicate entity identifiers"));
  Check(Op->FindUnit(2)->VehicleId==600 && Op->FindUnit(2)->SeatIndex==0 &&
   Op->Vehicle->Occupants[0]==2 && Op->FindUnit(2)->GetAttachParentActor()==Op->Vehicle,
   TEXT("Load restores both sides of driver occupancy and the actual attachment"));
  const FBGUnitRecord* Saved=Checkpoint->Units.FindByPredicate([](const FBGUnitRecord& U){return U.Id==1;});
  Check(Saved && Op->FindUnit(1)->Health==Saved->Health &&
   Op->FindUnit(1)->Inventory[0].Ammo==Saved->Inventory[0].Ammo &&
   Op->FindUnit(1)->ReloadRemaining==Saved->ReloadRemaining,
   TEXT("Injury, ammunition and an active reload survive the native disk roundtrip"));
  Check(!Op->FindUnit(4)->Alive() && Op->FindUnit(4)->Inventory[0].WeaponId==TEXT("compact_sidearm"),
   TEXT("A casualty remains dead with its equipment"));
  auto* Gate=Cast<ABGDoor>(Op->FindEntity(700));
  Check(Gate && Gate->bOpen && Gate->Health<90,TEXT("Barrier damage and open state persist"));
  Check(Op->Credits==0 && !Op->bRewardSettled && Op->Outcome==EBGOutcome::Active,
   TEXT("Midmission loading does not create credits or settle the objective"));
  Check(Op->AlarmSeconds==Checkpoint->AlarmSeconds && Op->ReportedPosition.Equals(Checkpoint->ReportedPosition,.01) &&
   Op->Reinforcements==Checkpoint->Reinforcements,TEXT("The active alarm, reported incident and response count survive loading"));
  const FBGUnitRecord* Queued=Checkpoint->Units.FindByPredicate([](const FBGUnitRecord& U){return U.Id==3;});
  Check(Queued && Op->FindUnit(3)->bHasOrder==Queued->HasOrder &&
   Op->FindUnit(3)->OrderQueue.Num()==Queued->Queue.Num() &&
   Op->FindUnit(3)->Order.Location.Equals(Queued->Order.Location, .01),
   TEXT("An active movement order and its queue persist without replaying interactions"));
  auto* Commander=Cast<ABGCommander>(UGameplayStatics::GetPlayerController(Op,0));
  Check(Commander && Commander->Selected.Num()==1 && Commander->Selected[0]->EntityId==2,
   TEXT("Selection resolves to restored operatives"));
  return !Failed;
 }
public:
 explicit FBGFoundationScenario(FAutomationTestBase* InTest):Test(InTest) {}
 virtual bool Update() override {
  if (Failed) return Finish(true);
  if (FPlatformTime::Seconds()-Began>240) {
   Test->AddError(TEXT("Runtime scenario exceeded its 240 second wall-clock budget."));
   return Finish(true);
  }
  ABGOperation* Op=Operation();
  if (!Op) return Expired(nullptr,30,TEXT("waiting for the actual DepotBlock game world"));
  ABGCommander* Commander=Cast<ABGCommander>(UGameplayStatics::GetPlayerController(Op,0));
  switch(Stage) {
  case 0: {
   if (!Commander || Op->Units.Num()!=17 || !Op->Vehicle || !Op->Specialist)
    return Expired(Op,30,TEXT("baseline actors and commander"));
   auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(Op->GetWorld());
   FNavLocation Point;
   if (!Nav || !Nav->ProjectPointToNavigation(FVector(-3500,-2150,20),Point,FVector(100,100,500)))
    return Expired(Op,30,TEXT("built runtime navigation"));
   Original.Reset(Op->Snapshot());
   FString Error;
   if (!Check(Op->ValidateSave(Original.Get(),Error),TEXT("Fresh native district is a valid persistent state: ")+Error)) return Finish(false);
   Check(Op->FindWeapon(TEXT("compact_sidearm")) && Op->FindWeapon(TEXT("compact_automatic")),
    TEXT("Both distinct data-defined weapons are available"));
   Commander->SelectNumber(0);
   Check(Commander->Selected.Num()==1 && Commander->Selected[0]->EntityId==1,TEXT("Individual selection addresses operative one"));
   Commander->SelectAll();
   Check(Commander->Selected.Num()==4,TEXT("Group selection addresses four operatives"));
   QuietFixtures(Op);
   for(int32 Id=1;Id<=4;++Id) StartLocations.Add(Op->FindUnit(Id)->GetActorLocation());
   Move(Op,1,FVector(-3500,-2150,20));
   Next(Op,TEXT("Native actor baseline, selection and navigation ready."));
   return false;
  }
  case 1: {
   if (!Near(Op,1,FVector(-3500,-2150,20))) return Expired(Op,15,TEXT("individual navigation order"));
   // Selection presentation must settle through ordinary controller/world ticks.
   if(SelectionOutlinePhase==0) {
    if(!VerifyOutlineState(Op,{1,2,3,4},{1,2,3,4},TEXT("Group selection after walking"))) return Finish(true);
    Commander->SelectNumber(0);
    OutlineChangedAt=Op->GetWorld()->GetTimeSeconds(); SelectionOutlinePhase=1;
    return false;
   }
   if(SelectionOutlinePhase==1) {
    if(Op->GetWorld()->GetTimeSeconds()-OutlineChangedAt<.1f) return false;
    if(!VerifyOutlineState(Op,{1,2,3,4},{1},TEXT("Individual selection after world ticks"))) return Finish(true);
    Commander->SelectAll();
    OutlineChangedAt=Op->GetWorld()->GetTimeSeconds(); SelectionOutlinePhase=2;
    return false;
   }
   if(SelectionOutlinePhase==2) {
    if(Op->GetWorld()->GetTimeSeconds()-OutlineChangedAt<.1f) return false;
    if(!VerifyOutlineState(Op,{1,2,3,4},{1,2,3,4},TEXT("Group reselection after world ticks"))) return Finish(true);
    SelectionOutlinePhase=3;
   }
   Check(FVector::Dist2D(Op->FindUnit(1)->GetActorLocation(),StartLocations[0])>350,TEXT("Individual movement advances through actual world ticks"));
   for(int32 Id=2;Id<=4;++Id)
    Check(FVector::Dist2D(Op->FindUnit(Id)->GetActorLocation(),StartLocations[Id-1])<80,TEXT("Unordered operatives hold their positions"));
   for(int32 Id=1;Id<=4;++Id) {
    const FVector Goal(-2200+((Id-1)%2)*135,-1950+((Id-1)/2)*135,20);
    Goals.Add(Goal); Move(Op,Id,Goal);
   }
   Next(Op,TEXT("Individual order moved only its recipient; issuing four formation destinations."));
   return false;
  }
  case 2: {
   for(int32 Id=1;Id<=4;++Id) if(!Near(Op,Id,Goals[Id-1])) return Expired(Op,20,TEXT("four operative movement"));
   for(int32 Id=1;Id<=4;++Id)
    Check(FVector::Dist2D(Op->FindUnit(Id)->GetActorLocation(),StartLocations[Id-1])>500,TEXT("Every group recipient moved using engine navigation"));
   Move(Op,1,FVector(-1700,-1750,20));
   Move(Op,1,FVector(-1700,-700,20),true);
   Check(Op->FindUnit(1)->OrderQueue.Num()==1,TEXT("Queued movement retains a second destination"));
   Next(Op,TEXT("Four operative navigation passed; testing queued street movement."));
   return false;
  }
  case 3: {
   if(!Near(Op,1,FVector(-1700,-700,20))) return Expired(Op,20,TEXT("queued movement through the junction"));
   Check(Op->FindUnit(1)->OrderQueue.IsEmpty(),TEXT("The first queued order progresses to the next"));
   for(int32 Id=1;Id<=4;++Id) Op->FindUnit(Id)->StopOrders(true);
   ABGUnit* Shooter=Op->FindUnit(1); ABGUnit* Target=Op->FindUnit(100);
   ABGUnit* Local=Op->FindUnit(200); ABGUnit* Distant=Op->FindUnit(201);
   // Explicit collision fixture placement, separate from all navigation assertions.
   Shooter->SetActorLocation(FVector(1350,-1350,110)); Target->SetActorLocation(FVector(1350,-650,110));
   Local->SetActorLocation(FVector(1950,-400,110)); Distant->SetActorLocation(FVector(4100,2400,110));
   Local->EvidenceSeconds=0; Local->ThreatId=0; Distant->EvidenceSeconds=0; Distant->ThreatId=0;
   const int32 Ammo=Shooter->Inventory[0].Ammo; const float Health=Target->Health;
   Shooter->ShotCooldown=0; Shooter->FireAt(Target);
   Check(Target->Health==Health && Shooter->Inventory[0].Ammo==Ammo,TEXT("Solid facility geometry prevents a shot and target damage"));
   Shooter->SetActorLocation(FVector(1750,-1350,110)); Target->SetActorLocation(FVector(1750,-650,110));
   ABGDoor* Gate=Cast<ABGDoor>(Op->FindEntity(700)); Gate->SetOpen(false);
   Shooter->ShotCooldown=0; Shooter->FireAt(Target);
   Check(Target->Health==Health && Gate->Health<90 && Shooter->Inventory[0].Ammo==Ammo-1,
    TEXT("Closed barrier intercepts the actual shot instead of damaging the intended person"));
   Check(Local->EvidenceSeconds>0 && Local->ThreatId==0,TEXT("A nearby guard hears a blocked incident without knowing the unseen shooter"));
   Check(Distant->EvidenceSeconds==0 && Distant->ThreatId==0,TEXT("A distant guard receives no global crime knowledge"));
   Gate->SetOpen(true); Shooter->ShotCooldown=0; Shooter->FireAt(Target);
   Check(Target->Health<Health && Shooter->Inventory[0].Ammo==Ammo-2,TEXT("Opening the barrier permits a collision-confirmed hit and consumes ammunition"));
   Check(Target->EvidenceSeconds>0,TEXT("A civilian reacts to the real nearby weapon event"));
   Check(Local->ThreatId==1 && Local->bHostile,TEXT("A witnessing guard identifies the actual shooter"));
   Shooter->bHolstered=true;
   Check(Local->ThreatId==1 && Local->bHostile,TEXT("Holstering preserves witnessed crime memory"));
   Op->AlarmSeconds=0;
   Commander->ApplySelection({Shooter});
   Commander->PauseTactical();
   PausedHealth=Target->Health; PausedAmmo=Shooter->Inventory[0].Ammo; PausedSimulation=Op->SimulationSeconds;
   PauseBegan=FPlatformTime::Seconds(); PausePhase=1; Shooter->ShotCooldown=0;
   FBGOrder Attack; Attack.Type=EBGOrderType::Attack; Attack.TargetId=100;
   Shooter->IssueOrder(Attack,false);
   Check(UGameplayStatics::IsGamePaused(Op),TEXT("The ordinary tactical pause control pauses the game"));
   Next(Op,TEXT("Geometry, interception and local evidence passed; verifying queued intent while genuinely paused."));
   return false;
  }
  case 4: {
   if(PausePhase==1) {
    if(FPlatformTime::Seconds()-PauseBegan<.25) return false;
    Check(Op->FindUnit(100)->Health==PausedHealth && Op->FindUnit(1)->Inventory[0].Ammo==PausedAmmo &&
     Op->SimulationSeconds==PausedSimulation,
     TEXT("Queued combat cannot apply damage, spend ammunition or advance simulation while paused"));
    Commander->PauseTactical(); PausePhase=2;
    return false;
   }
   if(PausePhase==2) {
    if(Op->FindUnit(100)->Health>=PausedHealth) return Expired(Op,5,TEXT("resuming the queued attack after tactical pause"));
    Check(Op->FindUnit(1)->Inventory[0].Ammo<PausedAmmo,TEXT("The prepared attack takes effect through actual ticks after unpausing"));
    Op->FindUnit(1)->StopOrders(true); Op->FindUnit(1)->bHolstered=true; Op->FindUnit(1)->UpdateAppearance();
    Op->AlarmSeconds=0; Op->FindUnit(4)->ReceiveHit(1000,nullptr);
    Check(FVector::Dist2D(Op->FindUnit(2)->EffectiveLocation(),Op->Vehicle->GetActorLocation())>600,
     TEXT("The boarding order starts well outside the vehicle interaction range"));
    Move(Op,3,FVector(-1900,-1100,20));
    FBGOrder Board; Board.Type=EBGOrderType::Board; Board.TargetId=600; Board.Location=Op->Vehicle->GetActorLocation();
    Op->FindUnit(2)->IssueOrder(Board,false); PausePhase=3;
    Test->AddInfo(TEXT("Tactical pause froze combat and the operation clock; prepared orders resumed through actual world ticks."));
   }
   if(!Op->FindUnit(2)->IsSeated()) return Expired(Op,20,TEXT("ordered approach and vehicle boarding"));
   if(!Near(Op,3,FVector(-1900,-1100,20))) return Expired(Op,20,TEXT("clearing the street through an actual squadmate movement order"));
   Test->AddInfo(TEXT("A real movement order cleared the living squadmate from the commanded vehicle route."));
   Check(Op->Vehicle->Occupants[0]==2 && Op->FindUnit(2)->SeatIndex==0,TEXT("Driver seat has one consistent owner"));
   VehicleStart=Op->Vehicle->GetActorLocation();
   if(!Check(Op->Vehicle->TravelTo(FVector(-1700,-1800,20)),TEXT("An occupied vehicle obtains a real street route"))) return Finish(true);
   Op->AlarmSeconds=18;
   TStrongObjectPtr<UBGSave> Moving(Op->Snapshot());
   if(!Check(Op->SaveOperation(Slot),TEXT("The native save accepts a moving occupied vehicle and active alarm"))) return Finish(true);
   Op->Vehicle->StopTravel(); Op->AlarmSeconds=0;
   if(!Check(Op->LoadOperation(Slot),TEXT("A moving vehicle save restores successfully"))) return Finish(true);
   Check(Op->Vehicle->bMoving && Op->Vehicle->Path.Num()==Moving->Vehicle.Path.Num() &&
    Op->Vehicle->PathIndex==Moving->Vehicle.PathIndex && Op->Vehicle->Occupants[0]==2 &&
    Op->FindUnit(2)->VehicleId==600 && Op->AlarmSeconds==18,
    TEXT("The saved active route, driver relationship and alarm resume before subsequent world ticks"));
   Op->AlarmSeconds=0; // Isolate vehicle movement from reinforcement scheduling.
   Next(Op,TEXT("Moving vehicle and alarm persistence passed; testing swept click-to-travel movement."));
   return false;
  }
  case 5: {
   if(CheckpointOutlinePending) {
    if(Op->GetWorld()->GetTimeSeconds()-OutlineChangedAt<.1f) return false;
    if(!VerifyOutlineState(Op,{1,3},{2},TEXT("Disk load excludes the seated driver and casualty after ticks"))) return Finish(true);
    CheckpointOutlinePending=false;
    Check(Op->RestoreOperation(Original.Get()),TEXT("The test resets its fixture without resurrecting a campaign casualty"));
    QuietFixtures(Op);
    Move(Op,1,FVector(1750,-1450,20));
    Move(Op,3,FVector(575,3000,312));
    Next(Op,TEXT("Occupancy, active reload, casualty, queued orders, barrier state, safe save/load and owned-body exclusion passed; walking the actual mission route."));
    return false;
   }
   if(Op->Vehicle->bMoving) return Expired(Op,15,TEXT("vehicle movement and stopping"));
   if(!Check(FVector::Dist2D(Op->Vehicle->GetActorLocation(),VehicleStart)>600 &&
    FVector::Dist2D(Op->Vehicle->GetActorLocation(),FVector(-1700,-1800,20))<180,
    TEXT("The vehicle actually follows its street route"))) return Finish(true);
   if(!VerifyOutlineState(Op,{1,3},{1},TEXT("Moving save restored; seated operative and casualty stay excluded"))) return Finish(true);
   if(!Check(Op->Vehicle->Exit(Op->FindUnit(2)),TEXT("A stopped vehicle provides a collision-clear navigable exit"))) return Finish(true);
   Check(Op->FindUnit(2)->VehicleId==0 && Op->Vehicle->Occupants[0]==0,TEXT("Exiting clears both seat relationships"));
   if(!Check(Op->Vehicle->Board(Op->FindUnit(2)),TEXT("The same operative can reboard safely"))) return Finish(true);
   Op->FindUnit(1)->ReceiveHit(13,nullptr); Op->FindUnit(1)->Reload();
   Move(Op,3,FVector(-1700,-1500,20)); Move(Op,3,FVector(-1700,-800,20),true);
   Commander->ApplySelection({Op->FindUnit(2)});
   Op->AlarmSeconds=22; Op->ReportedPosition=Op->FindUnit(1)->GetActorLocation();
   Checkpoint.Reset(Op->Snapshot());
   if(!Check(Op->SaveOperation(Slot),TEXT("A representative active mission writes a native checksummed save"))) return Finish(true);
   Op->FindUnit(1)->Health=12; Op->FindUnit(1)->Inventory[0].Ammo=0; Op->AlarmSeconds=0; Op->ReportedPosition=FVector::ZeroVector;
   Cast<ABGDoor>(Op->FindEntity(700))->SetOpen(false);
   Op->Vehicle->SetActorLocation(Op->Vehicle->GetActorLocation()+FVector(120,0,0));
   if(!Check(Op->LoadOperation(Slot),TEXT("The native disk save loads successfully")) || !VerifyCheckpoint(Op)) return Finish(true);
   TStrongObjectPtr<UBGSave> Bad(Op->Snapshot()); Bad->Units[1].Id=Bad->Units[0].Id;
   FString Error;
   Check(!Op->ValidateSave(Bad.Get(),Error) && !Error.IsEmpty(),TEXT("Duplicate saved identifiers are rejected with an explanation"));
   Check(!Op->RestoreOperation(Bad.Get()),TEXT("An invalid relationship graph cannot replace the live mission"));
   VerifyCheckpoint(Op);
   Check(Op->SaveOperation(Slot),TEXT("A repeated safe write retains a previous valid test save"));
   const FString SavePath=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames/NativeFoundationTest.sav"));
   TArray<uint8> Corrupt={0,1,2,3};
   Check(FFileHelper::SaveArrayToFile(Corrupt,*SavePath),TEXT("Only the isolated automation slot is corrupted for the recovery test"));
   Check(!Op->LoadOperation(Slot),TEXT("A damaged native save is rejected"));
   VerifyCheckpoint(Op);
   Check(FPaths::FileExists(SavePath+TEXT(".previous")),TEXT("A previous valid save remains available"));
   CheckpointOutlinePending=true; OutlineChangedAt=Op->GetWorld()->GetTimeSeconds();
   Test->AddInfo(TEXT("Representative save restored a selected driver and a dead operative; waiting for ordinary outline cleanup ticks."));
   return false;
  }
  case 6: {
   if(!Near(Op,1,FVector(1750,-1450,20))) return Expired(Op,35,TEXT("walking from deployment to the facility"));
   if(!VerifyOutlineState(Op,{1,2,3,4},Original->SelectedIds,TEXT("Restored on-foot bodies recover their selection outline after navigation"))) return Finish(true);
   FBGOrder Open; Open.Type=EBGOrderType::Interact; Open.TargetId=700;
   Op->FindUnit(1)->IssueOrder(Open,false);
   Next(Op,TEXT("The escort operative reached the facility through navigation, without teleportation."));
   return false;
  }
  case 7: {
   if(!Cast<ABGDoor>(Op->FindEntity(700))->bOpen) return Expired(Op,10,TEXT("contextual gate interaction"));
   Move(Op,1,FVector(3100,600,20));
   Next(Op,TEXT("A real interaction opened the gate; the operative now has an ordinary facility-entry movement order."));
   return false;
  }
  case 8: {
   if(!Near(Op,1,FVector(3100,600,20))) return Expired(Op,20,TEXT("navigation through the opened facility gate"));
   FBGOrder Acquire; Acquire.Type=EBGOrderType::Interact; Acquire.TargetId=500;
   Op->FindUnit(1)->IssueOrder(Acquire,false);
   Next(Op,TEXT("The operative crossed the opened gate and approached the laboratory entrance."));
   return false;
  }
  case 9: {
   if(!Op->bSpecialistAcquired || !Near(Op,3,FVector(575,3000,312)))
    return Expired(Op,35,TEXT("specialist acquisition and accessible raised route"));
   Check(Op->SpecialistLeaderId==1,TEXT("A contextual acquisition establishes a real escort relationship"));
   Check(Op->FindUnit(3)->GetActorLocation().Z>350,TEXT("An independent operative navigates onto the raised bridge using its ramps"));
   Op->FindUnit(3)->StopOrders(true);
   Move(Op,1,Op->ExtractionCenter+FVector(-67.5f,-67.5f,0));
   Next(Op,TEXT("Specialist acquired through orders; a second operative reached the raised route. Leader and specialist are walking to extraction first."));
   return false;
  }
  case 10: {
   if(ExtractionPhase==0) {
    if(!Near(Op,1,Op->ExtractionCenter,650) || !Near(Op,500,Op->ExtractionCenter,650))
     return Expired(Op,90,TEXT("leader and specialist walking to the extraction zone"));
    if(!Check(Op->Outcome==EBGOutcome::Active && Op->Credits==0 && !Op->bRewardSettled,
     TEXT("Partial extraction cannot silently recover other living operatives or award revenue"))) return Finish(true);
    for(int32 Id=2;Id<=4;++Id) if(Op->FindUnit(Id)->Alive())
     Move(Op,Id,Op->ExtractionCenter+FVector(((Id-1)%2-.5f)*135,((Id-1)/2-.5f)*135,0));
    ExtractionPhase=1; StageWorldTime=Op->GetWorld()->GetTimeSeconds(); StageBegan=FPlatformTime::Seconds();
    Test->AddInfo(TEXT("A real partial extraction remained active without revenue; the other three survivors now have actual extraction movement orders."));
   }
   if(Op->Outcome==EBGOutcome::Active) return Expired(Op,60,TEXT("remaining survivors walking to complete extraction"));
   if(!Check(Op->Outcome==EBGOutcome::Success && Op->bRewardSettled && Op->Credits==6000,
    TEXT("Real navigation and escort complete the mission and settle exactly one reward"))) return Finish(true);
   Check(Op->SaveOperation(Slot) && Op->LoadOperation(Slot),TEXT("A completed objective survives the native save roundtrip"));
   for(int32 Repeat=0;Repeat<10;++Repeat) Op->EvaluateMission();
   Check(Op->Credits==6000 && Op->bRewardSettled,TEXT("Loading and reevaluating success cannot duplicate revenue"));
   TStrongObjectPtr<UBGSave> Outside(Op->Snapshot());
   FBGUnitRecord* OutsideAgent=Outside->Units.FindByPredicate([](const FBGUnitRecord& U){return U.Id==4;});
   OutsideAgent->Transform.SetLocation(FVector(-3450,-2530,88));
   FString OutcomeError;
   Check(!Op->ValidateSave(Outside.Get(),OutcomeError),TEXT("A success save cannot abandon a living operative outside extraction"));
   TStrongObjectPtr<UBGSave> NoSurvivors(Op->Snapshot());
   NoSurvivors->SelectedIds.Reset(); NoSurvivors->SpecialistLeaderId=0;
   for(FBGUnitRecord& Record : NoSurvivors->Units) if(Record.Id>=1 && Record.Id<=4) {
    Record.Health=0; Record.HasOrder=false; Record.Queue.Reset(); Record.Hold=false;
   }
   Check(!Op->ValidateSave(NoSurvivors.Get(),OutcomeError),TEXT("A zero-survivor mission cannot load as a rewarded success"));
   Check(Op->RestoreOperation(Original.Get()),TEXT("Resetting the vehicle failure fixture succeeds"));
   ABGUnit* Driver=Op->FindUnit(2);
   // Deliberate occupancy fixture placement; travel above was proven by actual ticks.
   Driver->SetActorLocation(Op->Vehicle->GetActorLocation()+FVector(210,0,0));
   if(!Check(Op->Vehicle->Board(Driver),TEXT("A fresh vehicle boards the destruction-test occupant"))) return Finish(true);
   AActor* Barrier=Op->GetWorld()->SpawnActor<AActor>();
   UBoxComponent* ExitBlocker=NewObject<UBoxComponent>(Barrier,TEXT("TestBlockedExits"));
   Barrier->SetRootComponent(ExitBlocker);
   ExitBlocker->SetBoxExtent(FVector(600,600,250));
   ExitBlocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
   ExitBlocker->SetCollisionObjectType(ECC_WorldStatic);
   ExitBlocker->SetCollisionResponseToAllChannels(ECR_Block);
   ExitBlocker->SetCanEverAffectNavigation(false);
   ExitBlocker->RegisterComponent();
   Barrier->SetActorLocation(Op->Vehicle->GetActorLocation());
   Check(!Op->Vehicle->Exit(Driver) && Driver->VehicleId==600 && Op->Vehicle->Occupants[0]==2,
    TEXT("Physically blocked exits retain both sides of occupancy"));
   Op->Vehicle->ReceiveHit(300);
   Check(Op->Vehicle->Health==0 && !Op->Vehicle->bMoving && Driver->Health>0 && Driver->Health<100 &&
    Driver->VehicleId==600 && Op->Vehicle->Occupants[0]==2,
    TEXT("Vehicle destruction injures its occupant and consistently retains a trapped survivor"));
   Check(!Op->Vehicle->TravelTo(FVector(-1700,-1800,20)),TEXT("A wreck cannot confer vehicle movement or invulnerability"));
   Barrier->SetActorEnableCollision(false); Barrier->Destroy();
   Check(Op->Vehicle->Exit(Driver) && Driver->VehicleId==0 && Op->Vehicle->Occupants[0]==0,
    TEXT("Clearing the actual collision obstruction permits exit from the wreck and releases the seat"));
   Check(Op->RestoreOperation(Original.Get()),TEXT("Resetting the failure test fixture succeeds"));
   Op->Specialist->ReceiveHit(1000,nullptr);
   Check(Op->Outcome==EBGOutcome::Failed && Op->Credits==0,TEXT("A dead mission-critical target produces explicit failure without a reward"));
   Check(Op->RestoreOperation(Original.Get()),TEXT("Resetting the roster-loss test fixture succeeds"));
   for(int32 Id=1;Id<=4;++Id) Op->FindUnit(Id)->ReceiveHit(1000,nullptr);
   Check(Op->Outcome==EBGOutcome::Failed && Op->Credits==0,TEXT("Losing the four deployed operatives produces explicit mission failure"));
   Check(Op->RestoreOperation(Original.Get()),TEXT("Restart fixture restoration succeeds"));
   RestartedWorld=Op->GetWorld();
   Commander=Cast<ABGCommander>(UGameplayStatics::GetPlayerController(Op,0));
   Commander->Restart();
   Next(Op,TEXT("Extraction, reward-once loading and explicit casualty failures passed; testing ordinary restart."));
   return false;
  }
  case 11: {
   if(Op->GetWorld()==RestartedWorld.Get()) return Expired(Op,30,TEXT("restart level transition"));
   if(RestartOutlinePhase==0) {
    Check(Op->Outcome==EBGOutcome::Active && Op->Units.Num()==17 && Op->Credits==0 &&
     !Op->bRewardSettled && Op->Specialist->Alive(),TEXT("The restart control creates a fresh playable operation"));
    QuietFixtures(Op);
    Commander=Cast<ABGCommander>(UGameplayStatics::GetPlayerController(Op,0));
    Commander->SelectAll();
    OutlineChangedAt=Op->GetWorld()->GetTimeSeconds(); RestartOutlinePhase=1;
    return false;
   }
   if(Op->GetWorld()->GetTimeSeconds()-OutlineChangedAt<.1f) return false;
   if(!VerifyOutlineState(Op,{1,2,3,4},{1,2,3,4},TEXT("Fresh restart after ordinary outline initialization ticks"))) return Finish(true);
   StartLocations.Reset();
   for(int32 Id=1;Id<=4;++Id) {
    StartLocations.Add(Op->FindUnit(Id)->GetActorLocation());
    Check(FVector::Dist2D(StartLocations.Last(),Op->Vehicle->GetActorLocation())>600,
     TEXT("Each default deployed operative must actually walk to board"));
   }
   Commander->BoardOrExit();
   Next(Op,TEXT("Restart created a fresh operation; the ordinary group boarding action now orders all four deployed operatives to the vehicle."));
   return false;
  }
  case 12: {
   bool AllSeated=true;
   for(int32 Id=1;Id<=4;++Id) AllSeated &= Op->FindUnit(Id)->VehicleId==600;
   if(!AllSeated) return Expired(Op,25,TEXT("all four default deployed operatives boarding through real group orders"));
   if(GroupSeatedAt<0) { GroupSeatedAt=Op->GetWorld()->GetTimeSeconds(); return false; }
   if(Op->GetWorld()->GetTimeSeconds()-GroupSeatedAt<.1f) return false;
   if(!VerifyOutlineState(Op,{}, {1,2,3,4},TEXT("All four seated bodies leave the outline pass after ordinary ticks"))) return Finish(true);
   TSet<int32> Seats;
   for(int32 Id=1;Id<=4;++Id) {
    ABGUnit* Unit=Op->FindUnit(Id);
    Check(Op->Vehicle->Occupants.IsValidIndex(Unit->SeatIndex) &&
     Op->Vehicle->Occupants[Unit->SeatIndex]==Id && Unit->GetAttachParentActor()==Op->Vehicle,
     TEXT("Group boarding maintains matching unit, seat and actual attachment state"));
    Seats.Add(Unit->SeatIndex);
   }
   Check(Seats.Num()==4,TEXT("All four operatives own distinct vehicle seats"));
   Commander=Cast<ABGCommander>(UGameplayStatics::GetPlayerController(Op,0));
   Commander->BoardOrExit();
   for(int32 Id=1;Id<=4;++Id)
    Check(Op->FindUnit(Id)->VehicleId==0 && !Op->Vehicle->Occupants.Contains(Id),
     TEXT("Ordinary group disembarking releases both sides of every seat"));
   BoardingClearGoal=Op->Vehicle->GetActorLocation()+FVector(190,-650,-90);
   if(!Move(Op,1,BoardingClearGoal)) return Finish(true);
   BoardingEdgePhase=-1;
   Next(Op,TEXT("All four operatives walked to board and disembarked consistently; operative 1 is walking clear of the boundary boarding ray."));
   return false;
  }
  case 13: {
   ABGUnit* Edge=Op->FindUnit(4);
   if(BoardingEdgePhase==-1) {
    if(!Near(Op,1,BoardingClearGoal)) return Expired(Op,12,TEXT("the exited operative walking clear of the boundary boarding ray"));
    if(!VerifyOutlineState(Op,{1,2,3,4},{1,2,3,4},TEXT("Group disembarking restores owned-body outlines through real navigation ticks"))) return Finish(true);
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(Op->GetWorld());
    FNavLocation Ground;
    const FVector Fixture=Op->Vehicle->GetActorLocation()+FVector(329.9,0,-90);
    if(!Check(Nav && Nav->ProjectPointToNavigation(Fixture,Ground,FVector(60,60,160)),
     TEXT("The boarding-boundary fixture has actual accessible ground"))) return Finish(true);
    // Placement establishes the boundary condition after a real order clears its obstruction.
    Edge->StopOrders(true);
    Edge->SetActorLocation(FVector(Fixture.X,Fixture.Y,
     Ground.Location.Z+Edge->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2));
    BoardingEdgePhase=0;
    StageWorldTime=Op->GetWorld()->GetTimeSeconds(); StageBegan=FPlatformTime::Seconds();
    Test->AddInfo(TEXT("Operative 1 actually walked clear before the height-sensitive boarding fixture was established."));
    return false;
   }
   if(BoardingEdgePhase==0) {
    if(Op->GetWorld()->GetTimeSeconds()-StageWorldTime<.15f) return false;
    const FVector Car=Op->Vehicle->GetActorLocation();
    const double Dz=FMath::Abs(Edge->GetActorLocation().Z-Car.Z);
    if(!Check(Dz>1 && Dz<100,TEXT("The settled boarding fixture has a real capsule-to-vehicle height difference"))) return Finish(true);
    const double SpatialBoundary=FMath::Sqrt(330.0*330.0-Dz*Dz);
    const double PlanarDistance=(SpatialBoundary+330.0)*.5;
    Edge->SetActorLocation(FVector(Car.X+PlanarDistance,Car.Y,Edge->GetActorLocation().Z));
    BoardingEdgeStart=Edge->GetActorLocation();
    if(!Check(FVector::Dist2D(BoardingEdgeStart,Car)<330 &&
     FVector::Dist(BoardingEdgeStart,Car)>330,
     TEXT("The real boundary fixture is within planar boarding range but outside spatial boarding range"))) return Finish(true);
    Commander=Cast<ABGCommander>(UGameplayStatics::GetPlayerController(Op,0));
    Commander->SelectNumber(3); Commander->BoardOrExit();
    if(!Check(Edge->bHasOrder && Edge->VehicleId==0,
     TEXT("The height-sensitive boarding order retains a movement approach instead of being cancelled"))) return Finish(true);
    BoardingEdgePhase=1;
    StageWorldTime=Op->GetWorld()->GetTimeSeconds(); StageBegan=FPlatformTime::Seconds();
    return false;
   }
   if(!Edge->IsSeated()) {
    BoardingEdgeTravel=FMath::Max(BoardingEdgeTravel,
     static_cast<float>(FVector::Dist2D(BoardingEdgeStart,Edge->GetActorLocation())));
    return Expired(Op,8,TEXT("the boundary boarding order making its real physical approach"));
   }
   Check(BoardingEdgeTravel>.1f,TEXT("Horizontal navigation was observed before the boundary operative boarded"));
   Check(Edge->VehicleId==600 && Op->Vehicle->Occupants.IsValidIndex(Edge->SeatIndex) &&
    Op->Vehicle->Occupants[Edge->SeatIndex]==4 && Edge->GetAttachParentActor()==Op->Vehicle,
    TEXT("The completed boundary approach owns one consistent vehicle seat"));
   Commander=Cast<ABGCommander>(UGameplayStatics::GetPlayerController(Op,0));
   Commander->BoardOrExit();
   Check(Edge->VehicleId==0 && !Op->Vehicle->Occupants.Contains(4),
    TEXT("The boundary operative disembarks with its seat released"));
   for(ABGUnit* Unit : Op->Units) if(Unit) Unit->SetActorTickEnabled(true);
   Check(Op->RestoreOperation(Original.Get()),TEXT("The controlled boarding fixture returns to the original active operation"));
   Test->AddInfo(TEXT("Native runtime scenario complete, including four-operative group boarding, the real height-sensitive approach and operative-only outline state across selection, casualties, vehicles, disk load and restart. Outline assertions inspect runtime properties; they are not shader, rendering, performance, packaging or manual-control evidence."));
   return Finish(false);
  }
  default: return Finish(true);
  }
 }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBGFoundationRuntimeTest,"Blackglass.Foundation.Runtime",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FBGFoundationRuntimeTest::RunTest(const FString& Parameters) {
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FBGFoundationScenario>(this));
 return true;
}
#endif