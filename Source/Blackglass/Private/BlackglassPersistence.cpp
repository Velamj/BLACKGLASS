#include "BlackglassGame.h"
#include "AIController.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
constexpr int32 SaveVersion = 1;
constexpr int32 MaxPayloadBytes = 4 * 1024 * 1024;
constexpr int32 EnvelopeSize = 16;
constexpr uint8 SaveMagic[] = {'B', 'G', 'O', 'P', '0', '0', '0', '1'};

bool Reject(FString& Error, const FString& Reason)
{
 Error = Reason;
 return false;
}

bool Bounded(float Value, float Minimum, float Maximum)
{
 return FMath::IsFinite(Value) && Value >= Minimum && Value <= Maximum;
}

bool PositionValid(const FVector& Value)
{
 return !Value.ContainsNaN() && Value.GetAbsMax() <= 1000000.0;
}

bool TransformValid(const FTransform& Value)
{
 return !Value.ContainsNaN() && PositionValid(Value.GetLocation()) &&
  Value.GetRotation().IsNormalized() && Value.GetScale3D().GetMin() > 0.01 &&
  Value.GetScale3D().GetMax() <= 4.0;
}

bool CanonicalRole(int32 Id, EBGRole& Role)
{
 if (Id >= 1 && Id <= 4) Role = EBGRole::Operative;
 else if (Id >= 100 && Id <= 107) Role = EBGRole::Civilian;
 else if (Id >= 200 && Id <= 207) Role = EBGRole::Guard;
 else if (Id == 500) Role = EBGRole::Specialist;
 else return false;
 return true;
}

bool AllowedSlot(const FString& Slot)
{
#if WITH_DEV_AUTOMATION_TESTS
 if (Slot == TEXT("NativeFoundationTest")) return true;
#endif
 return Slot.Equals(TEXT("Quick"), ESearchCase::IgnoreCase) ||
  Slot.Equals(TEXT("Autosave"), ESearchCase::IgnoreCase) ||
  Slot.Equals(TEXT("Slot1"), ESearchCase::IgnoreCase) ||
  Slot.Equals(TEXT("Slot2"), ESearchCase::IgnoreCase) ||
  Slot.Equals(TEXT("Slot3"), ESearchCase::IgnoreCase);
}

FString SlotPath(const FString& Slot)
{
 FString Canonical = Slot;
 if (Slot.Equals(TEXT("Quick"), ESearchCase::IgnoreCase)) Canonical = TEXT("Quick");
 else if (Slot.Equals(TEXT("Autosave"), ESearchCase::IgnoreCase)) Canonical = TEXT("Autosave");
 return FPaths::ConvertRelativePathToFull(
  FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), Canonical + TEXT(".sav")));
}

void AppendWord(TArray<uint8>& Bytes, uint32 Value)
{
 for (int32 Index = 0; Index < 4; ++Index)
  Bytes.Add(static_cast<uint8>((Value >> (Index * 8)) & 0xffu));
}

uint32 ReadWord(const TArray<uint8>& Bytes, int32 Offset)
{
 uint32 Value = 0;
 for (int32 Index = 0; Index < 4; ++Index)
  Value |= static_cast<uint32>(Bytes[Offset + Index]) << (Index * 8);
 return Value;
}

bool EncodeSave(UBGSave* Data, TArray<uint8>& Bytes, FString& Error)
{
 TArray<uint8> Payload;
 if (!UGameplayStatics::SaveGameToMemory(Data, Payload) ||
  Payload.Num() <= 0 || Payload.Num() > MaxPayloadBytes)
  return Reject(Error, TEXT("Could not serialize the operation within its save limit."));
 Bytes.Reset();
 Bytes.Append(SaveMagic, UE_ARRAY_COUNT(SaveMagic));
 AppendWord(Bytes, static_cast<uint32>(Payload.Num()));
 AppendWord(Bytes, FCrc::MemCrc32(Payload.GetData(), Payload.Num()));
 Bytes.Append(Payload);
 return true;
}

UBGSave* DecodeSave(const TArray<uint8>& Bytes, FString& Error)
{
 if (Bytes.Num() < EnvelopeSize ||
  FMemory::Memcmp(Bytes.GetData(), SaveMagic, UE_ARRAY_COUNT(SaveMagic)) != 0)
 {
  Error = TEXT("This is not a compatible BLACKGLASS operation save.");
  return nullptr;
 }
 const uint32 Size = ReadWord(Bytes, 8);
 if (Size == 0 || Size > static_cast<uint32>(MaxPayloadBytes) ||
  Bytes.Num() - EnvelopeSize != static_cast<int32>(Size) ||
  FCrc::MemCrc32(Bytes.GetData() + EnvelopeSize, Size) != ReadWord(Bytes, 12))
 {
  Error = TEXT("Save length or checksum is invalid; the current operation was preserved.");
  return nullptr;
 }
 TArray<uint8> Payload;
 Payload.Append(Bytes.GetData() + EnvelopeSize, static_cast<int32>(Size));
 UBGSave* Result = Cast<UBGSave>(UGameplayStatics::LoadGameFromMemory(Payload));
 if (!Result || Result->GetClass() != UBGSave::StaticClass())
  Error = TEXT("The save does not contain a BLACKGLASS operation.");
 return Result && Result->GetClass() == UBGSave::StaticClass() ? Result : nullptr;
}

bool ReadSaveFile(const FString& Path, TArray<uint8>& Bytes, FString& Error)
{
 const int64 Size = IFileManager::Get().FileSize(*Path);
 if (Size < 0) return Reject(Error, TEXT("The selected save slot does not exist."));
 if (Size < EnvelopeSize || Size > MaxPayloadBytes + EnvelopeSize)
  return Reject(Error, TEXT("The save file has an invalid size."));
 if (!FFileHelper::LoadFileToArray(Bytes, *Path))
  return Reject(Error, TEXT("The save file could not be read."));
 return true;
}

bool PublishFile(const FString& Temporary, const FString& Destination, FString& Error)
{
#if PLATFORM_WINDOWS
 // Same-directory Windows rename/replace: never use cross-volume COPY_ALLOWED.
 // WRITE_THROUGH is requested; this does not promise recovery from every power loss.
 if (::MoveFileExW(*Temporary, *Destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
  return true;
 return Reject(Error, FString::Printf(
  TEXT("Windows could not replace the save (error %lu); the prior slot was retained."),
  static_cast<unsigned long>(::GetLastError())));
#else
 // Platform file-manager behavior is not claimed to be atomic.
 if (IFileManager::Get().Move(*Destination, *Temporary, true, false, false, true))
  return true;
 return Reject(Error, TEXT("Could not replace the save; the prior slot was retained."));
#endif
}

bool OrderValid(const FBGOrder& Order, const TMap<int32, const FBGUnitRecord*>& Units)
{
 if (!PositionValid(Order.Location) || !Bounded(Order.NavigationRetryRemaining, 0, 3))
  return false;
 if (Order.NavigationRetryStarted && Order.Type != EBGOrderType::Move) return false;
 if (!Order.NavigationRetryStarted && Order.NavigationRetryRemaining != 0) return false;
 switch (Order.Type)
 {
 case EBGOrderType::Move: return Order.TargetId == 0;
 case EBGOrderType::Attack:
  return Units.Contains(Order.TargetId) || Order.TargetId == 600 ||
   Order.TargetId == 700 || Order.TargetId == 701;
 case EBGOrderType::Board: return Order.TargetId == 600;
 case EBGOrderType::Interact:
  return Order.TargetId == 500 || Order.TargetId == 700 || Order.TargetId == 701;
 default: return false;
 }
}
}

UBGSave* ABGOperation::Snapshot() const
{
 UBGSave* Data = NewObject<UBGSave>();
 Data->Version = SaveVersion;
 Data->MapId = TEXT("DepotBlock_v1");
 for (const ABGUnit* Unit : Units)
  if (IsValid(Unit)) Data->Units.Add(Unit->Record());
 if (IsValid(Vehicle)) Data->Vehicle = Vehicle->Record();
 for (const ABGDoor* Door : Doors)
 {
  if (!IsValid(Door)) continue;
  FBGDoorRecord Record;
  Record.Id = Door->EntityId;
  Record.Open = Door->bOpen;
  Record.Health = Door->Health;
  Data->Doors.Add(Record);
 }
 Data->SimulationSeconds = SimulationSeconds;
 Data->AlarmSeconds = AlarmSeconds;
 Data->ReportedPosition = ReportedPosition;
 Data->SpecialistLeaderId = SpecialistLeaderId;
 Data->SpecialistAcquired = bSpecialistAcquired;
 Data->Outcome = Outcome;
 Data->RewardSettled = bRewardSettled;
 Data->Credits = Credits;
 Data->RandomSeed = Random.GetCurrentSeed();
 Data->Reinforcements = Reinforcements;
 Data->CameraRotation = FRotator(-55, -45, 0);
 const ABGCommander* Commander = Cast<ABGCommander>(
  UGameplayStatics::GetPlayerController(this, 0));
 if (Commander)
 {
  for (const ABGUnit* Unit : Commander->Selected)
   if (IsValid(Unit) && Unit->UnitRole == EBGRole::Operative && Unit->Alive())
    Data->SelectedIds.AddUnique(Unit->EntityId);
  const ABGCamera* Rig = Cast<ABGCamera>(Commander->GetPawn());
  if (Rig && Rig->Camera)
  {
   Data->CameraLocation = Rig->GetActorLocation();
   Data->CameraWidth = Rig->Camera->OrthoWidth;
   Data->CameraRotation = Rig->Camera->GetRelativeRotation();
  }
 }
 return Data;
}

bool ABGOperation::ValidateSave(const UBGSave* Data, FString& Error) const
{
 Error.Empty();
 if (!IsValid(Data) || Data->Version != SaveVersion ||
  Data->MapId != TEXT("DepotBlock_v1"))
  return Reject(Error, TEXT("Save version or district is incompatible."));
 if (!Bounded(Data->SimulationSeconds, 0, 315360000) ||
  !Bounded(Data->AlarmSeconds, 0, 86400) || !PositionValid(Data->ReportedPosition))
  return Reject(Error, TEXT("Simulation time or alarm state is invalid."));
 if (Data->Reinforcements < 0 || Data->Reinforcements > 4 ||
  Data->Units.Num() != 17 + Data->Reinforcements)
  return Reject(Error, TEXT("The saved district entity count is inconsistent."));
 if (static_cast<uint8>(Data->Outcome) > static_cast<uint8>(EBGOutcome::Aborted))
  return Reject(Error, TEXT("The saved mission outcome is invalid."));
 const bool Won = Data->Outcome == EBGOutcome::Success;
 if (Data->RewardSettled != Won || Data->Credits != (Won ? 6000 : 0))
  return Reject(Error, TEXT("Mission settlement and credit totals disagree."));
 if (!PositionValid(Data->CameraLocation) ||
  !Bounded(Data->CameraWidth, 1800, 8500) || Data->CameraRotation.ContainsNaN() ||
  FMath::Abs(Data->CameraRotation.Pitch) > 360000 ||
  FMath::Abs(Data->CameraRotation.Yaw) > 360000 ||
  FMath::Abs(Data->CameraRotation.Roll) > 360000)
  return Reject(Error, TEXT("Saved camera state is invalid."));

 TMap<int32, const FBGUnitRecord*> ById;
 for (const FBGUnitRecord& Unit : Data->Units)
 {
  EBGRole ExpectedRole;
  if (!CanonicalRole(Unit.Id, ExpectedRole) || Unit.Role != ExpectedRole ||
   ById.Contains(Unit.Id) ||
   (Unit.Id >= 204 && Unit.Id <= 207 && Unit.Id >= 204 + Data->Reinforcements))
   return Reject(Error, TEXT("Unit identifiers or roles are inconsistent."));
  if (!TransformValid(Unit.Transform) || !Bounded(Unit.Health, 0, 100) ||
   Unit.Label.IsEmpty() || Unit.Label.Len() > 128 ||
   !PositionValid(Unit.LastKnown) || !Bounded(Unit.Evidence, 0, 86400) ||
   !Bounded(Unit.Cooldown, 0, 120) || !Bounded(Unit.ReloadRemaining, 0, 120))
   return Reject(Error, FString::Printf(TEXT("Unit %d has invalid state."), Unit.Id));
  if (Unit.Inventory.Num() > 8 || Unit.WeaponIndex < 0 ||
   (Unit.Inventory.IsEmpty() ? Unit.WeaponIndex != 0 : Unit.WeaponIndex >= Unit.Inventory.Num()))
   return Reject(Error, TEXT("Inventory slots or selected equipment are invalid."));
  float Weight = 0;
  for (const FBGItem& Item : Unit.Inventory)
  {
   if (Item.WeaponId.IsNone())
   {
    if (Item.Ammo != 0 || Item.Reserve != 0)
     return Reject(Error, TEXT("An empty inventory slot contains ammunition."));
    continue;
   }
   const FBGWeaponDefinition* Definition = FindWeapon(Item.WeaponId);
   if (!Definition || !Bounded(Definition->Weight, 0, 24) ||
    Definition->Magazine <= 0 || Definition->Reserve < 0 ||
    Item.Ammo < 0 || Item.Ammo > Definition->Magazine ||
    Item.Reserve < 0 || Item.Reserve > Definition->Reserve)
    return Reject(Error, TEXT("Equipment or ammunition is incompatible with the loaded definitions."));
   Weight += Definition->Weight;
  }
  if (!Bounded(Weight, 0, 24))
   return Reject(Error, TEXT("A saved inventory exceeds carrying capacity."));
  if (Unit.PatrolPoints.Num() > 128 || Unit.PatrolIndex < 0 ||
   Unit.PatrolIndex >= (Unit.PatrolPoints.IsEmpty() ? 1 : Unit.PatrolPoints.Num()) ||
   Unit.Queue.Num() > 64 || (!Unit.HasOrder && !Unit.Queue.IsEmpty()))
   return Reject(Error, TEXT("Patrol progress or queued orders are invalid."));
  for (const FVector& Point : Unit.PatrolPoints)
   if (!PositionValid(Point)) return Reject(Error, TEXT("A patrol route contains invalid coordinates."));
  if (Unit.VehicleId == 0 ? Unit.Seat != INDEX_NONE :
   (Unit.VehicleId != 600 || Unit.Seat < 0 || Unit.Seat >= 6))
   return Reject(Error, TEXT("A unit's vehicle relationship is invalid."));
  ById.Add(Unit.Id, &Unit);
 }
 for (int32 Id = 1; Id <= 4; ++Id)
  if (!ById.Contains(Id)) return Reject(Error, TEXT("An operative record is missing."));
 for (int32 Id = 100; Id <= 107; ++Id)
  if (!ById.Contains(Id)) return Reject(Error, TEXT("A civilian record is missing."));
 for (int32 Id = 200; Id < 204 + Data->Reinforcements; ++Id)
  if (!ById.Contains(Id)) return Reject(Error, TEXT("A security record is missing."));
 if (!ById.Contains(500)) return Reject(Error, TEXT("The specialist record is missing."));
 for (const FBGUnitRecord& Unit : Data->Units)
 {
  if (!OrderValid(Unit.Order, ById) ||
   (Unit.ThreatId != 0 && !ById.Contains(Unit.ThreatId)))
   return Reject(Error, TEXT("An order or security memory references a missing entity."));
  for (const FBGOrder& Queued : Unit.Queue)
   if (!OrderValid(Queued, ById))
    return Reject(Error, TEXT("A queued order references an invalid target."));
 }

 const FBGVehicleRecord& Car = Data->Vehicle;
 if (Car.Id != 600 || !TransformValid(Car.Transform) ||
  !Bounded(Car.Health, 0, 300) || Car.Occupants.Num() != 6 ||
  Car.Path.Num() > 1024 || Car.PathIndex < 0 || Car.PathIndex > Car.Path.Num())
  return Reject(Error, TEXT("Vehicle state is invalid."));
 for (const FVector& Point : Car.Path)
  if (!PositionValid(Point)) return Reject(Error, TEXT("Vehicle route contains invalid coordinates."));
 TSet<int32> Seated;
 for (int32 Seat = 0; Seat < 6; ++Seat)
 {
  const int32 Id = Car.Occupants[Seat];
  if (Id == 0) continue;
  const FBGUnitRecord* const* Record = ById.Find(Id);
  if (!Record || Seated.Contains(Id) || (*Record)->VehicleId != 600 || (*Record)->Seat != Seat)
   return Reject(Error, TEXT("Vehicle occupants and unit seat ownership disagree."));
  Seated.Add(Id);
 }
 for (const FBGUnitRecord& Unit : Data->Units)
  if (Unit.VehicleId == 600 && Car.Occupants[Unit.Seat] != Unit.Id)
   return Reject(Error, TEXT("A unit is seated without a matching vehicle occupant."));
 if (Car.Moving)
 {
  const FBGUnitRecord* const* Driver = ById.Find(Car.Occupants[0]);
  if (Car.Health <= 0 || Car.PathIndex >= Car.Path.Num() || !Driver || (*Driver)->Health <= 0)
   return Reject(Error, TEXT("A moving vehicle has no valid route or living driver."));
 }
 if (Data->Doors.Num() != 2)
  return Reject(Error, TEXT("Saved access gates are missing."));
 TSet<int32> GateIds;
 for (const FBGDoorRecord& Door : Data->Doors)
 {
  if ((Door.Id != 700 && Door.Id != 701) || GateIds.Contains(Door.Id) ||
   !Bounded(Door.Health, 0, 90) || (Door.Health <= 0 && !Door.Open))
   return Reject(Error, TEXT("Gate identifiers or destruction state are invalid."));
  GateIds.Add(Door.Id);
 }

 if ((!Data->SpecialistAcquired && Data->SpecialistLeaderId != 0) ||
  (Data->SpecialistLeaderId != 0 &&
   (Data->SpecialistLeaderId < 1 || Data->SpecialistLeaderId > 4)))
  return Reject(Error, TEXT("Specialist control references an invalid operative."));
 // A secured specialist may lose its leader and be reacquired by another operative.
 // Dead-leader records can exist between the actor hit and the next operation tick.
 if (Won && (!Data->SpecialistAcquired || ById.FindChecked(500)->Health <= 0))
  return Reject(Error, TEXT("The successful outcome has no living acquired specialist."));
 if (Won)
 {
  if (!PositionValid(ExtractionCenter) || !Bounded(ExtractionRadius, 1, 1000000))
   return Reject(Error, TEXT("The district extraction configuration is invalid."));
  const auto Extracted = [&](const FBGUnitRecord& Unit)
  {
   const FVector Location = Unit.VehicleId == 600 ?
    Car.Transform.GetLocation() : Unit.Transform.GetLocation();
   return FVector::DistSquared2D(Location, ExtractionCenter) <= FMath::Square(ExtractionRadius);
  };
  if (!Extracted(*ById.FindChecked(500)))
   return Reject(Error, TEXT("The successful outcome has no extracted specialist."));
  int32 Survivors = 0;
  for (int32 Id = 1; Id <= 4; ++Id)
  {
   const FBGUnitRecord& Unit = *ById.FindChecked(Id);
   if (Unit.Health <= 0) continue;
   ++Survivors;
   if (!Extracted(Unit))
    return Reject(Error, TEXT("The successful outcome leaves a surviving operative outside extraction."));
  }
  if (Survivors == 0)
   return Reject(Error, TEXT("The successful outcome has no surviving operative."));
 }
 if (Data->SelectedIds.Num() > 4)
  return Reject(Error, TEXT("The saved selection exceeds the deployed squad."));
 TSet<int32> Selection;
 for (int32 Id : Data->SelectedIds)
 {
  const FBGUnitRecord* const* Unit = ById.Find(Id);
  if (!Unit || Id < 1 || Id > 4 || (*Unit)->Health <= 0 || Selection.Contains(Id))
   return Reject(Error, TEXT("The selection contains an invalid or duplicate operative."));
  Selection.Add(Id);
 }
 return true;
}

bool ABGOperation::SaveOperation(const FString& Slot)
{
 FString Error;
 if (!AllowedSlot(Slot))
 {
  Notify(TEXT("Save failed: choose Quick, Autosave, or Slot1–Slot3."));
  return false;
 }
 if (bRestoring)
 {
  Notify(TEXT("Save unavailable while an operation is being restored."));
  return false;
 }
 TStrongObjectPtr<UBGSave> Data(Snapshot());
 if (!ValidateSave(Data.Get(), Error))
 {
  Notify(TEXT("Save failed: ") + Error);
  return false;
 }
 TArray<uint8> Bytes;
 if (!EncodeSave(Data.Get(), Bytes, Error))
 {
  Notify(TEXT("Save failed: ") + Error);
  return false;
 }
 const FString Destination = SlotPath(Slot);
 if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination), true))
 {
  Notify(TEXT("Save failed: the save directory could not be created."));
  return false;
 }
 const FString Temporary = Destination + TEXT(".") + FGuid::NewGuid().ToString() + TEXT(".tmp");
 auto Stage = [&](const FString& Path, const TArray<uint8>& Contents) -> bool
 {
  if (!FFileHelper::SaveArrayToFile(
   TArrayView64<const uint8>(Contents.GetData(), Contents.Num()), *Path))
   return Reject(Error, TEXT("Could not write the staged save."));
  TArray<uint8> ReadBack;
  if (!ReadSaveFile(Path, ReadBack, Error)) return false;
  if (ReadBack != Contents) return Reject(Error, TEXT("Staged save bytes did not match their source."));
  TStrongObjectPtr<UBGSave> Verified(DecodeSave(ReadBack, Error));
  return Verified.IsValid() && ValidateSave(Verified.Get(), Error);
 };
 if (!Stage(Temporary, Bytes))
 {
  IFileManager::Get().Delete(*Temporary, false, true, true);
  Notify(TEXT("Save failed during verification: ") + Error);
  return false;
 }
 TArray<uint8> PreviousBytes;
 FString PreviousError;
 if (ReadSaveFile(Destination, PreviousBytes, PreviousError))
 {
  TStrongObjectPtr<UBGSave> Previous(DecodeSave(PreviousBytes, PreviousError));
  if (Previous.IsValid() && ValidateSave(Previous.Get(), PreviousError))
  {
   const FString Backup = Destination + TEXT(".previous");
   const FString BackupTemp = Temporary + TEXT(".previous");
   if (!Stage(BackupTemp, PreviousBytes) || !PublishFile(BackupTemp, Backup, Error))
   {
    IFileManager::Get().Delete(*BackupTemp, false, true, true);
    IFileManager::Get().Delete(*Temporary, false, true, true);
    Notify(TEXT("Save failed while retaining its previous valid copy: ") + Error);
    return false;
   }
  }
 }
 if (!PublishFile(Temporary, Destination, Error))
 {
  IFileManager::Get().Delete(*Temporary, false, true, true);
  Notify(TEXT("Save failed: ") + Error);
  return false;
 }
 if (!Slot.Equals(TEXT("Autosave"), ESearchCase::IgnoreCase))
  Notify(TEXT("Operation saved: ") + Slot);
 return true;
}

bool ABGOperation::LoadOperation(const FString& Slot)
{
 if (!AllowedSlot(Slot))
 {
  Notify(TEXT("Load failed: choose Quick, Autosave, or Slot1–Slot3."));
  return false;
 }
 if (bRestoring)
 {
  Notify(TEXT("Load unavailable while an operation is being restored."));
  return false;
 }
 FString Error;
 TArray<uint8> Bytes;
 if (!ReadSaveFile(SlotPath(Slot), Bytes, Error))
 {
  Notify(TEXT("Load failed: ") + Error);
  return false;
 }
 TStrongObjectPtr<UBGSave> Data(DecodeSave(Bytes, Error));
 if (!Data.IsValid() || !ValidateSave(Data.Get(), Error))
 {
  Notify(TEXT("Load failed: ") + Error);
  return false;
 }
 if (!RestoreOperation(Data.Get())) return false;
 Notify(TEXT("Operation loaded: ") + Slot);
 return true;
}

bool ABGOperation::RestoreOperation(const UBGSave* Data)
{
 FString Error;
 if (bRestoring || !ValidateSave(Data, Error))
 {
  Notify(TEXT("Load failed: ") + (Error.IsEmpty() ? TEXT("restore already in progress.") : Error));
  return false;
 }
 if (!GetWorld() || !IsValid(Vehicle) || Vehicle->EntityId != 600 || Doors.Num() != 2)
 {
  Notify(TEXT("Load failed: the district must be initialized first."));
  return false;
 }
 TMap<int32, ABGUnit*> Live;
 for (ABGUnit* Unit : Units)
 {
  EBGRole ExpectedRole;
  if (!IsValid(Unit) || !CanonicalRole(Unit->EntityId, ExpectedRole) ||
   Unit->UnitRole != ExpectedRole || Live.Contains(Unit->EntityId))
  {
   Notify(TEXT("Load failed: current district entities are inconsistent; restart the operation."));
   return false;
  }
  Live.Add(Unit->EntityId, Unit);
 }
 TMap<int32, ABGDoor*> LiveDoors;
 for (ABGDoor* Door : Doors)
 {
  if (!IsValid(Door) || LiveDoors.Contains(Door->EntityId))
  {
   Notify(TEXT("Load failed: current access gates are inconsistent."));
   return false;
  }
  LiveDoors.Add(Door->EntityId, Door);
 }
 for (const FBGDoorRecord& Door : Data->Doors)
  if (!LiveDoors.Contains(Door.Id))
  {
   Notify(TEXT("Load failed: an authored access gate is missing."));
   return false;
  }
 for (const FBGUnitRecord& Unit : Data->Units)
  if (!Live.Contains(Unit.Id) && (Unit.Id < 204 || Unit.Id > 207))
  {
   Notify(TEXT("Load failed: an authored district actor is missing; restart first."));
   return false;
  }

 TGuardValue<bool> Restoring(bRestoring, true);
 TArray<ABGUnit*> Created;
 for (const FBGUnitRecord& Unit : Data->Units)
 {
  if (Live.Contains(Unit.Id)) continue;
  ABGUnit* Spawned = SpawnUnit(Unit.Id, Unit.Role, Unit.Label, Unit.Transform.GetLocation());
  if (!Spawned)
  {
   for (ABGUnit* NewUnit : Created)
   {
    Units.Remove(NewUnit);
    if (AAIController* AI = Cast<AAIController>(NewUnit->GetController())) AI->Destroy();
    NewUnit->Destroy();
   }
   Notify(TEXT("Load failed: a response unit could not be restored."));
   return false;
  }
  Created.Add(Spawned);
  Live.Add(Unit.Id, Spawned);
 }
 TSet<int32> Desired;
 for (const FBGUnitRecord& Unit : Data->Units) Desired.Add(Unit.Id);
 for (int32 Index = Units.Num() - 1; Index >= 0; --Index)
 {
  ABGUnit* Unit = Units[Index];
  if (Desired.Contains(Unit->EntityId)) continue;
  // Extra response actors from a later state must not duplicate after loading.
  if (AAIController* AI = Cast<AAIController>(Unit->GetController())) AI->Destroy();
  Unit->Destroy();
  Units.RemoveAt(Index);
 }
 for (ABGUnit* Unit : Units)
 {
  if (AAIController* AI = Cast<AAIController>(Unit->GetController())) AI->StopMovement();
  Unit->GetCharacterMovement()->StopMovementImmediately();
  Unit->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
  Unit->SetSelected(false);
 }
 for (const FBGUnitRecord& Record : Data->Units)
  Live.FindChecked(Record.Id)->RestoreRecord(Record);
 for (const FBGDoorRecord& Record : Data->Doors)
 {
  ABGDoor* Door = LiveDoors.FindChecked(Record.Id);
  Door->Health = Record.Health;
  Door->SetOpen(Record.Open);
 }
 Vehicle->RestoreRecord(Data->Vehicle);
 Vehicle->RestoreSeats();
 Specialist = Live.FindChecked(500);
 SimulationSeconds = Data->SimulationSeconds;
 AlarmSeconds = Data->AlarmSeconds;
 ReportedPosition = Data->ReportedPosition;
 SpecialistLeaderId = Data->SpecialistLeaderId;
 bSpecialistAcquired = Data->SpecialistAcquired;
 Outcome = Data->Outcome;
 bRewardSettled = Data->RewardSettled;
 Credits = Data->Credits;
 Reinforcements = Data->Reinforcements;
 Random.Initialize(Data->RandomSeed);

 // Reissue only locomotion after relationships are resolved. IssueOrder immediately
 // processes interactions/shots, which would repeat effects during loading. Other
 // pending intent and its exact queue resume through Think on the next simulation tick.
 for (const FBGUnitRecord& Record : Data->Units)
 {
  ABGUnit* Unit = Live.FindChecked(Record.Id);
  Unit->ThinkAccumulator = 0;
  if (Outcome == EBGOutcome::Active && Record.Health > 0 &&
   Record.VehicleId == 0 && Record.HasOrder && !Record.Hold &&
   Record.Order.Type == EBGOrderType::Move)
   Unit->MoveTo(Record.Order.Location);
 }
 ABGCommander* Commander = Cast<ABGCommander>(
  UGameplayStatics::GetPlayerController(this, 0));
 if (Commander)
 {
  TArray<ABGUnit*> Selected;
  for (int32 Id : Data->SelectedIds) Selected.Add(Live.FindChecked(Id));
  Commander->ApplySelection(Selected);
  Commander->bDragging = false;
  ABGCamera* Rig = Cast<ABGCamera>(Commander->GetPawn());
  if (Rig && Rig->Camera)
  {
   Rig->SetActorLocation(Data->CameraLocation);
   Rig->Camera->SetOrthoWidth(Data->CameraWidth);
   Rig->Camera->SetRelativeRotation(Data->CameraRotation);
   Rig->Camera->SetRelativeLocation(-Data->CameraRotation.Vector() * 5500.0);
  }
 }
 return true;
}
