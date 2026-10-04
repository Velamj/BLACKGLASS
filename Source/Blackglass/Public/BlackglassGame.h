#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/SaveGame.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "BlackglassGame.generated.h"

class UStaticMeshComponent;
class UCameraComponent;
class USpringArmComponent;
class UBoxComponent;
class ABGOperation;
class ABGVehicle;
class ABGUnit;

UENUM(BlueprintType)
enum class EBGRole : uint8 { Operative, Civilian, Guard, Specialist };
UENUM(BlueprintType)
enum class EBGOrderType : uint8 { Move, Attack, Board, Interact };
UENUM(BlueprintType)
enum class EBGOutcome : uint8 { Active, Success, Failed, Aborted };

USTRUCT(BlueprintType)
struct FBGOrder {
 GENERATED_BODY()
 UPROPERTY() EBGOrderType Type = EBGOrderType::Move;
 UPROPERTY() FVector Location = FVector::ZeroVector;
 UPROPERTY() int32 TargetId = 0;
 UPROPERTY() float NavigationRetryRemaining = 0;
 UPROPERTY() bool NavigationRetryStarted = false;
};
USTRUCT(BlueprintType)
struct FBGWeaponDefinition {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Id;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Label;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float Range = 1200;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 18;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float Interval = .55f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float HearingRadius = 1600;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float Weight = 2;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float SpreadDegrees = 1;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float ReloadSeconds = 1.6f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Magazine = 12;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Reserve = 96;
};
USTRUCT(BlueprintType)
struct FBGItem {
 GENERATED_BODY()
 UPROPERTY() FName WeaponId;
 UPROPERTY() int32 Ammo = 0;
 UPROPERTY() int32 Reserve = 0;
};
USTRUCT()
struct FBGUnitRecord {
 GENERATED_BODY()
 UPROPERTY() int32 Id = 0;
 UPROPERTY() EBGRole Role = EBGRole::Civilian;
 UPROPERTY() FString Label;
 UPROPERTY() FTransform Transform;
 UPROPERTY() float Health = 100;
 UPROPERTY() TArray<FBGItem> Inventory;
 UPROPERTY() int32 WeaponIndex = 0;
 UPROPERTY() bool Holstered = true;
 UPROPERTY() bool Hold = false;
 UPROPERTY() bool Hostile = false;
 UPROPERTY() bool HasOrder = false;
 UPROPERTY() FBGOrder Order;
 UPROPERTY() TArray<FBGOrder> Queue;
 UPROPERTY() int32 ThreatId = 0;
 UPROPERTY() FVector LastKnown = FVector::ZeroVector;
 UPROPERTY() float Evidence = 0;
 UPROPERTY() float Cooldown = 0;
 UPROPERTY() float ReloadRemaining = 0;
 UPROPERTY() int32 VehicleId = 0;
 UPROPERTY() int32 Seat = INDEX_NONE;
 UPROPERTY() TArray<FVector> PatrolPoints;
 UPROPERTY() int32 PatrolIndex = 0;
};
USTRUCT()
struct FBGVehicleRecord {
 GENERATED_BODY()
 UPROPERTY() int32 Id = 600;
 UPROPERTY() FTransform Transform;
 UPROPERTY() float Health = 300;
 UPROPERTY() TArray<int32> Occupants;
 UPROPERTY() TArray<FVector> Path;
 UPROPERTY() int32 PathIndex = 0;
 UPROPERTY() bool Moving = false;
};
USTRUCT()
struct FBGDoorRecord {
 GENERATED_BODY()
 UPROPERTY() int32 Id = 0;
 UPROPERTY() bool Open = false;
 UPROPERTY() float Health = 90;
};
UCLASS()
class BLACKGLASS_API UBGSave : public USaveGame {
 GENERATED_BODY()
public:
 UPROPERTY() int32 Version = 1;
 UPROPERTY() FString MapId = TEXT("DepotBlock_v1");
 UPROPERTY() TArray<FBGUnitRecord> Units;
 UPROPERTY() FBGVehicleRecord Vehicle;
 UPROPERTY() TArray<FBGDoorRecord> Doors;
 UPROPERTY() float SimulationSeconds = 0;
 UPROPERTY() float AlarmSeconds = 0;
 UPROPERTY() FVector ReportedPosition = FVector::ZeroVector;
 UPROPERTY() int32 SpecialistLeaderId = 0;
 UPROPERTY() bool SpecialistAcquired = false;
 UPROPERTY() EBGOutcome Outcome = EBGOutcome::Active;
 UPROPERTY() bool RewardSettled = false;
 UPROPERTY() int32 Credits = 0;
 UPROPERTY() int32 RandomSeed = 1337;
 UPROPERTY() int32 Reinforcements = 0;
 UPROPERTY() TArray<int32> SelectedIds;
 UPROPERTY() FVector CameraLocation = FVector::ZeroVector;
 UPROPERTY() FRotator CameraRotation = FRotator::ZeroRotator;
 UPROPERTY() float CameraWidth = 4200;
};

UCLASS()
class BLACKGLASS_API ABGUnit : public ACharacter {
 GENERATED_BODY()
public:
 ABGUnit();
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 void Initialize(int32 NewId, EBGRole NewRole, const FString& NewLabel);
 void IssueOrder(const FBGOrder& NewOrder, bool Append);
 void StopOrders(bool Hold = false);
 void SwitchWeapon();
 void Reload();
 void ReceiveHit(float Amount, ABGUnit* Source);
 void HearIncident(const FVector& Location, int32 SourceId);
 void UpdateAppearance();
 void SetSelected(bool Selected);
 FVector EffectiveLocation() const;
 ABGOperation* Operation() const;
 const FBGWeaponDefinition* WeaponDefinition() const;
 bool Alive() const { return Health > 0; }
 bool IsSeated() const { return VehicleId != 0; }
 float InventoryWeight() const;
 FBGUnitRecord Record() const;
 void RestoreRecord(const FBGUnitRecord& Data);
 UPROPERTY() int32 EntityId = 0;
 UPROPERTY() EBGRole UnitRole = EBGRole::Civilian;
 UPROPERTY() FString Label;
 UPROPERTY() float Health = 100;
 UPROPERTY() TArray<FBGItem> Inventory;
 UPROPERTY() int32 WeaponIndex = 0;
 UPROPERTY() bool bHolstered = true;
 UPROPERTY() bool bHold = false;
 UPROPERTY() bool bHostile = false;
 UPROPERTY() bool bHasOrder = false;
 UPROPERTY() FBGOrder Order;
 UPROPERTY() TArray<FBGOrder> OrderQueue;
 UPROPERTY() int32 ThreatId = 0;
 UPROPERTY() FVector LastKnown = FVector::ZeroVector;
 UPROPERTY() float EvidenceSeconds = 0;
 UPROPERTY() float ShotCooldown = 0;
 UPROPERTY() float ReloadRemaining = 0;
 UPROPERTY() int32 VehicleId = 0;
 UPROPERTY() int32 SeatIndex = INDEX_NONE;
 UPROPERTY() TArray<FVector> PatrolPoints;
 UPROPERTY() int32 PatrolIndex = 0;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftLeg;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> RightLeg;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftArm;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> RightArm;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> WeaponMesh;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> SelectionRing;
 float ThinkAccumulator = 0;
 float GaitPhase = 0;
 float Recoil = 0;
 bool bSelection = false;
 void Think(float Interval);
 void ProcessOrder();
 void FireAt(ABGUnit* Target);
 void FireAtEntity(AActor* Target);
 bool MoveTo(const FVector& Destination);
};

UCLASS()
class BLACKGLASS_API ABGVehicle : public APawn {
 GENERATED_BODY()
public:
 ABGVehicle();
 virtual void Tick(float DeltaSeconds) override;
 bool Board(ABGUnit* Unit);
 bool Exit(ABGUnit* Unit);
 bool TravelTo(const FVector& Destination);
 void StopTravel();
 void ReceiveHit(float Amount);
 bool HasSelectedDriver(const TArray<TObjectPtr<ABGUnit>>& Selected) const;
 FBGVehicleRecord Record() const;
 void RestoreRecord(const FBGVehicleRecord& Data);
 void RestoreSeats();
 UPROPERTY() int32 EntityId = 600;
 UPROPERTY() float Health = 300;
 UPROPERTY() TObjectPtr<UBoxComponent> Hull;
 UPROPERTY() TArray<int32> Occupants;
 UPROPERTY() TArray<FVector> Path;
 UPROPERTY() int32 PathIndex = 0;
 UPROPERTY() bool bMoving = false;
 float Speed = 650;
};
UCLASS()
class BLACKGLASS_API ABGDoor : public AActor {
 GENERATED_BODY()
public:
 ABGDoor();
 void Toggle();
 void SetOpen(bool Open);
 void ReceiveHit(float Amount);
 UPROPERTY() int32 EntityId = 0;
 UPROPERTY() bool bOpen = false;
 UPROPERTY() float Health = 90;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Leaf;
};
UCLASS()
class BLACKGLASS_API ABGNavBounds : public ANavMeshBoundsVolume {
 GENERATED_BODY()
public:
 ABGNavBounds();
 UPROPERTY() TObjectPtr<UBoxComponent> Bounds;
};

UCLASS()
class BLACKGLASS_API ABGCamera : public APawn {
 GENERATED_BODY()
public:
 ABGCamera();
 UPROPERTY() TObjectPtr<UCameraComponent> Camera;
 void Zoom(float Steps);
};
UCLASS()
class BLACKGLASS_API ABGCommander : public APlayerController {
 GENERATED_BODY()
public:
 ABGCommander();
 virtual void BeginPlay() override;
 virtual void SetupInputComponent() override;
 virtual void PlayerTick(float DeltaSeconds) override;
 void LeftDown();
 void LeftUp();
 void ContextOrder();
 void SelectAll();
 void SelectNumber(int32 Index);
 void Stop();
 void Hold();
 void SwitchWeapon();
 void Reload();
 void Holster();
 void BoardOrExit();
 void PauseTactical();
 void SaveQuick();
 void LoadQuick();
 void Restart();
 void Abort();
 void Recenter();
 void ZoomIn();
 void ZoomOut();
 void RotateCamera();
 void ApplySelection(const TArray<ABGUnit*>& Units, bool Append = false);
 ABGOperation* Operation() const;
 UPROPERTY() TArray<TObjectPtr<ABGUnit>> Selected;
 bool bDragging = false;
 bool bTracking = false;
 FVector2D DragStart = FVector2D::ZeroVector;
 FVector2D DragEnd = FVector2D::ZeroVector;
 void SelectOne();
 void SelectTwo();
 void SelectThree();
 void SelectFour();
};
UCLASS()
class BLACKGLASS_API ABGHUD : public AHUD {
 GENERATED_BODY()
public:
 virtual void DrawHUD() override;
 bool ConsumeClick(const FVector2D& ScreenPoint);
 float UIScale() const;
};

UCLASS()
class BLACKGLASS_API ABGOperation : public AGameModeBase {
 GENERATED_BODY()
public:
 ABGOperation();
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 void BuildDistrict();
 void LoadDefinitions();
 ABGUnit* SpawnUnit(int32 Id, EBGRole NewRole, const FString& Label, const FVector& Location);
 ABGUnit* FindUnit(int32 Id) const;
 AActor* FindEntity(int32 Id) const;
 bool CanSee(const ABGUnit* Observer, const ABGUnit* Target) const;
 bool IsDetected(const ABGUnit* Target) const;
 void ReportIncident(const FVector& Location, int32 SourceId, float Radius);
 void AcquireSpecialist(ABGUnit* Controller);
 void EvaluateMission();
 void Fail(const FString& Reason);
 void Notify(const FString& Message);
 void RestartOperation();
 const FBGWeaponDefinition* FindWeapon(FName Id) const;
 UBGSave* Snapshot() const;
 bool ValidateSave(const UBGSave* Data, FString& Error) const;
 bool SaveOperation(const FString& Slot);
 bool LoadOperation(const FString& Slot);
 bool RestoreOperation(const UBGSave* Data);
 UPROPERTY() TArray<TObjectPtr<ABGUnit>> Units;
 UPROPERTY() TArray<TObjectPtr<ABGDoor>> Doors;
 UPROPERTY() TObjectPtr<ABGVehicle> Vehicle;
 UPROPERTY() TObjectPtr<ABGUnit> Specialist;
 UPROPERTY(EditAnywhere) TArray<FBGWeaponDefinition> Weapons;
 UPROPERTY(EditAnywhere) bool bFriendlyFire = true;
 UPROPERTY(EditAnywhere) bool bReducedFlash = false;
 UPROPERTY() EBGOutcome Outcome = EBGOutcome::Active;
 UPROPERTY() bool bSpecialistAcquired = false;
 UPROPERTY() int32 SpecialistLeaderId = 0;
 UPROPERTY() float SimulationSeconds = 0;
 UPROPERTY() float AlarmSeconds = 0;
 UPROPERTY() FVector ReportedPosition = FVector::ZeroVector;
 UPROPERTY() bool bRewardSettled = false;
 UPROPERTY() int32 Credits = 0;
 UPROPERTY() int32 Reinforcements = 0;
 UPROPERTY() FString Notice;
 UPROPERTY() float NoticeExpires = 0;
 UPROPERTY() FVector ExtractionCenter = FVector(-3800, 2700, 0);
 UPROPERTY() float ExtractionRadius = 650;
 FRandomStream Random;
 bool bRestoring = false;
};