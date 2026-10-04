#include "BlackglassGame.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Texture.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace {
AStaticMeshActor* Block(UWorld* World, const FVector& Position, const FVector& Size,
    const FLinearColor& Color, bool Collision = true, const FRotator& Rotation = FRotator::ZeroRotator, bool Asphalt = false) {
    AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Position, Rotation);
    if (!Actor) return nullptr;
    UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
    Mesh->SetMobility(EComponentMobility::Static);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Actor->SetActorScale3D(Size / 100.f);
    Mesh->SetCollisionProfileName(Collision ? TEXT("BlockAll") : TEXT("NoCollision"));
    Mesh->SetCanEverAffectNavigation(Collision);
    if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_BlackglassSurface.M_BlackglassSurface"))) {
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, Actor);
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Material->SetScalarParameterValue(TEXT("Roughness"), Asphalt ? .94f : .84f);
        Material->SetScalarParameterValue(TEXT("DetailScale"), FMath::Clamp(static_cast<float>(FMath::Max(Size.X, Size.Y) / 180.), 1.f, 70.f));
        if (Asphalt) Material->SetTextureParameterValue(TEXT("SurfaceDetail"),
            LoadObject<UTexture>(nullptr, TEXT("/Game/Textures/T_BG_AsphaltDetail.T_BG_AsphaltDetail")));
        Mesh->SetMaterial(0, Material);
    }
    Actor->Tags.Add(TEXT("BlackglassDistrict"));
    return Actor;
}

// Static visual attachments share material batches; they never supply cover or navigation.
struct FBlackglassDressing {
    AActor* Owner = nullptr;
    UStaticMesh* Cube = nullptr;
    UMaterialInterface* Surface = nullptr;
    TMap<FName, UInstancedStaticMeshComponent*> Batches;
    int32 Instances = 0;
    explicit FBlackglassDressing(UWorld* World) {
        Owner = World->SpawnActor<AActor>();
        if (!Owner) return;
        Owner->Tags.Add(TEXT("BlackglassVisualDressing"));
        USceneComponent* Root = NewObject<USceneComponent>(Owner, TEXT("VisualAttachmentsRoot"));
        Root->SetMobility(EComponentMobility::Static);
        Owner->SetRootComponent(Root);
        Owner->AddInstanceComponent(Root);
        Root->RegisterComponent();
        Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        Surface = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_BlackglassSurface.M_BlackglassSurface"));
    }
    void Add(FName Key, const FVector& Position, const FVector& Size, const FLinearColor& Color,
        float Roughness = .8f, float DetailStrength = 0.f, FRotator Rotation = FRotator::ZeroRotator) {
        if (!Owner || !Cube || !Surface) return;
        UInstancedStaticMeshComponent* Batch = Batches.FindRef(Key);
        if (!Batch) {
            Batch = NewObject<UInstancedStaticMeshComponent>(Owner, Key);
            Owner->AddInstanceComponent(Batch);
            Batch->SetupAttachment(Owner->GetRootComponent());
            Batch->SetMobility(EComponentMobility::Static);
            Batch->SetStaticMesh(Cube);
            Batch->SetCollisionProfileName(TEXT("NoCollision"));
            Batch->SetCanEverAffectNavigation(false);
            UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Surface, Owner);
            Material->SetVectorParameterValue(TEXT("Color"), Color);
            Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
            Material->SetScalarParameterValue(TEXT("DetailStrength"), DetailStrength);
            Material->SetScalarParameterValue(TEXT("DetailScale"), 2.f);
            Batch->SetMaterial(0, Material);
            Batch->RegisterComponent();
            Batches.Add(Key, Batch);
        }
        Batch->AddInstance(FTransform(Rotation.Quaternion(), Position, Size / 100.f), true);
        ++Instances;
    }
    void Facade(const FVector& Center, const FVector& Size) {
        const FLinearColor Window(.075f, .16f, .18f), Frame(.065f, .075f, .08f);
        const FLinearColor Trim(.48f, .48f, .43f), Equipment(.24f, .29f, .3f);
        const float Ground = Center.Z - Size.Z * .5f;
        auto Face = [&](const FVector& Origin, float Width, const FRotator& Rotation) {
            const int32 Columns = FMath::Clamp(FMath::FloorToInt(Width / 310.f), 2, 7);
            const int32 Rows = Size.Z >= 500.f ? 2 : 1;
            auto Place = [&](FName Key, FVector Local, FVector Dimensions, FLinearColor Color, float Roughness) {
                Add(Key, Origin + Rotation.RotateVector(Local), Dimensions, Color, Roughness, 0.f, Rotation);
            };
            for (int32 Column = 0; Column < Columns; ++Column) {
                const float X = (Column + .5f) * Width / Columns - Width * .5f;
                for (int32 Row = 0; Row < Rows; ++Row) {
                    const float Z = Ground + 175.f + Row * 190.f - Origin.Z;
                    Place(TEXT("OpaqueGlazing"), FVector(X, 0, Z), FVector(120, 4, 120), Window, .3f);
                    Place(TEXT("WindowFrames"), FVector(X - 63, 0, Z), FVector(6, 8, 130), Frame, .58f);
                    Place(TEXT("WindowFrames"), FVector(X + 63, 0, Z), FVector(6, 8, 130), Frame, .58f);
                    Place(TEXT("WindowFrames"), FVector(X, 0, Z - 63), FVector(132, 8, 6), Frame, .58f);
                    Place(TEXT("WindowFrames"), FVector(X, 0, Z + 63), FVector(132, 8, 6), Frame, .58f);
                }
                if (Column > 0) Place(TEXT("PanelSeams"), FVector(X - Width / Columns * .5f, 0, Ground + Size.Z * .5f - Origin.Z),
                    FVector(4, 5, Size.Z - 20), Frame, .58f);
            }
            Place(TEXT("ConcreteTrim"), FVector(0, 0, Ground + 45 - Origin.Z), FVector(Width, 9, 28), Trim, .88f);
            Place(TEXT("ConcreteTrim"), FVector(0, 0, Ground + Size.Z - 18 - Origin.Z), FVector(Width + 15, 14, 24), Trim, .88f);
        };
        Face(Center + FVector(0, -Size.Y * .5f - 3, 0), Size.X, FRotator::ZeroRotator);
        Face(Center + FVector(0, Size.Y * .5f + 3, 0), Size.X, FRotator::ZeroRotator);
        Face(Center + FVector(-Size.X * .5f - 3, 0, 0), Size.Y, FRotator(0, 90, 0));
        Face(Center + FVector(Size.X * .5f + 3, 0, 0), Size.Y, FRotator(0, 90, 0));
        const float Roof = Center.Z + Size.Z * .5f;
        for (int32 Index = 0; Index < 3; ++Index) {
            const FVector Vent = FVector(Center.X + (Index - 1) * Size.X * .22f, Center.Y, Roof + 45);
            Add(TEXT("RoofEquipment"), Vent, FVector(150, 170, 90), Equipment, .68f, .3f);
            Add(TEXT("RoofEquipment"), Vent + FVector(0, 0, 49), FVector(166, 186, 8), Equipment, .68f, .3f);
            for (int32 Slat = 0; Slat < 5; ++Slat)
                Add(TEXT("WindowFrames"), Vent + FVector(-56 + Slat * 28.f, 0, 54), FVector(9, 150, 4), Frame, .58f);
        }
    }
};

bool InsideExtraction(const ABGOperation* Operation, const FVector& Location) {
    return FVector::DistSquared2D(Location, Operation->ExtractionCenter) <= FMath::Square(Operation->ExtractionRadius);
}
}

ABGNavBounds::ABGNavBounds() {
    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("RegisteredNavigationExtent"));
    Bounds->SetupAttachment(GetRootComponent());
    Bounds->SetBoxExtent(FVector(5300, 4300, 1200));
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bounds->SetCanEverAffectNavigation(false);
    Bounds->SetHiddenInGame(true);
}

ABGOperation::ABGOperation() {
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = ABGCamera::StaticClass();
    PlayerControllerClass = ABGCommander::StaticClass();
    HUDClass = ABGHUD::StaticClass();
    Random.Initialize(1337);
}

void ABGOperation::BeginPlay() {
    Super::BeginPlay();
    LoadDefinitions();
    BuildDistrict();
    Notify(TEXT("Acquire the research specialist. Escort them to northwest extraction. Approach and interact to secure the escort."));
}

void ABGOperation::LoadDefinitions() {
    Weapons.Reset();
    FString Text;
    if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Weapons.csv")))) {
        TArray<FString> Lines;
        Text.ParseIntoArrayLines(Lines, true);
        for (int32 Index = 1; Index < Lines.Num(); ++Index) {
            TArray<FString> Fields;
            Lines[Index].ParseIntoArray(Fields, TEXT(","), false);
            if (Fields.Num() != 11) continue;
            for (FString& Field : Fields) Field.TrimStartAndEndInline();
            FBGWeaponDefinition Definition;
            Definition.Id = FName(*Fields[0]); Definition.Label = Fields[1];
            float RangeMeters = 0, HearingMeters = 0;
            const bool Valid = LexTryParseString(RangeMeters, *Fields[2])
                && LexTryParseString(Definition.Damage, *Fields[3])
                && LexTryParseString(Definition.Interval, *Fields[4])
                && LexTryParseString(HearingMeters, *Fields[5])
                && LexTryParseString(Definition.Weight, *Fields[6])
                && LexTryParseString(Definition.Magazine, *Fields[7])
                && LexTryParseString(Definition.Reserve, *Fields[8])
                && LexTryParseString(Definition.SpreadDegrees, *Fields[9])
                && LexTryParseString(Definition.ReloadSeconds, *Fields[10]);
            if (!Valid || Definition.Id.IsNone() || FindWeapon(Definition.Id)
                || !FMath::IsFinite(RangeMeters) || RangeMeters <= 0 || RangeMeters > 100
                || !FMath::IsFinite(HearingMeters) || HearingMeters < 0 || HearingMeters > 200
                || !FMath::IsFinite(Definition.Damage) || Definition.Damage <= 0 || Definition.Damage > 500
                || !FMath::IsFinite(Definition.Interval) || Definition.Interval < .05f || Definition.Interval > 10
                || !FMath::IsFinite(Definition.Weight) || Definition.Weight <= 0 || Definition.Weight > 30
                || Definition.Magazine <= 0 || Definition.Magazine > 500 || Definition.Reserve < 0 || Definition.Reserve > 5000
                || !FMath::IsFinite(Definition.SpreadDegrees) || Definition.SpreadDegrees < 0 || Definition.SpreadDegrees > 45
                || !FMath::IsFinite(Definition.ReloadSeconds) || Definition.ReloadSeconds < .1f || Definition.ReloadSeconds > 20) continue;
            Definition.Range = RangeMeters * 100; Definition.HearingRadius = HearingMeters * 100;
            Weapons.Add(Definition);
        }
    }
    if (!FindWeapon(TEXT("compact_sidearm")) || !FindWeapon(TEXT("compact_automatic"))) {
        Weapons.Reset();
        FBGWeaponDefinition Sidearm;
        Sidearm.Id = TEXT("compact_sidearm"); Sidearm.Label = TEXT("Vesper compact sidearm");
        Sidearm.Range = 1200; Sidearm.Damage = 24; Sidearm.Interval = .55f;
        Sidearm.HearingRadius = 1600; Sidearm.Weight = 2; Sidearm.Magazine = 12; Sidearm.Reserve = 72;
        Sidearm.SpreadDegrees = 1.1f; Sidearm.ReloadSeconds = 1.5f;
        Weapons.Add(Sidearm);
        FBGWeaponDefinition Automatic = Sidearm;
        Automatic.Id = TEXT("compact_automatic"); Automatic.Label = TEXT("Rook compact automatic");
        Automatic.Range = 2000; Automatic.Damage = 10; Automatic.Interval = .14f;
        Automatic.HearingRadius = 2300; Automatic.Weight = 4; Automatic.Magazine = 30;
        Automatic.Reserve = 150; Automatic.SpreadDegrees = 2.5f; Automatic.ReloadSeconds = 2.2f;
        Weapons.Add(Automatic);
        UE_LOG(LogTemp, Warning, TEXT("BLACKGLASS weapon data unavailable or incomplete; using explicit foundation definitions."));
    }
}

const FBGWeaponDefinition* ABGOperation::FindWeapon(FName Id) const {
    for (const FBGWeaponDefinition& Definition : Weapons) if (Definition.Id == Id) return &Definition;
    return nullptr;
}

void ABGOperation::BuildDistrict() {
    UWorld* World = GetWorld();
    // Procedural runtime geometry uses dynamic lighting; it has no baked lightmaps.
    if (AWorldSettings* DistrictSettings = World->GetWorldSettings()) DistrictSettings->bForceNoPrecomputedLighting = true;
    const FLinearColor Concrete(.18f, .19f, .18f), Dark(.08f, .11f, .13f), Steel(.15f, .18f, .19f);
    const FLinearColor Road(.025f, .033f, .039f), Pavement(.22f, .22f, .19f), Mark(.66f, .61f, .43f);
    // Presentation-only continuation: the playable collision ground remains the original block.
    Block(World, FVector(0, 0, -120), FVector(18000, 15000, 20), FLinearColor(.07f, .09f, .09f), false);
    Block(World, FVector(0, 0, -50), FVector(9600, 7600, 100), Concrete);
    Block(World, FVector(-1700, 0, 6), FVector(1000, 7400, 12), Road, true, FRotator::ZeroRotator, true);
    Block(World, FVector(0, -1800, 6), FVector(9400, 1000, 12), Road, true, FRotator::ZeroRotator, true);
    Block(World, FVector(-2450, 2200, 6), FVector(4700, 1000, 12), Road, true, FRotator::ZeroRotator, true);
    Block(World, FVector(-2450, 0, 12), FVector(400, 7300, 24), Pavement);
    Block(World, FVector(-950, 0, 12), FVector(400, 7300, 24), Pavement);
    Block(World, FVector(0, -2500, 12), FVector(9300, 400, 24), Pavement);
    Block(World, FVector(-2600, 1500, 12), FVector(4100, 350, 24), Pavement);
    for (int32 Index = -7; Index <= 7; ++Index) {
        Block(World, FVector(-1700, Index * 470.f, 14), FVector(15, 170, 3), Mark, false);
        Block(World, FVector(Index * 590.f, -1800, 14), FVector(200, 15, 3), Mark, false);
    }
    // Low, bounded buildings preserve the foundation camera's tactical readability.
    Block(World, FVector(-3750, -650, 270), FVector(1500, 1700, 540), Dark);
    Block(World, FVector(-3700, 650, 200), FVector(1550, 550, 400), Concrete);
    Block(World, FVector(-3400, -3300, 210), FVector(2200, 600, 420), Steel);
    Block(World, FVector(1500, -3250, 260), FVector(3100, 650, 520), Dark);
    Block(World, FVector(200, 1250, 280), FVector(1200, 2200, 560), Steel);
    for (int32 Index = 0; Index < 5; ++Index)
        Block(World, FVector(-2980, -1180 + Index * 280.f, 280), FVector(12, 100, 210), Mark, false);
    // Facility perimeter: main gate south and narrow service access west.
    Block(World, FVector(1337.5f, -1000, 130), FVector(475, 50, 260), Concrete);
    Block(World, FVector(3062.5f, -1000, 130), FVector(2275, 50, 260), Concrete);
    Block(World, FVector(1100, 237.5f, 130), FVector(50, 2475, 260), Concrete);
    Block(World, FVector(1100, 2212.5f, 130), FVector(50, 775, 260), Concrete);
    Block(World, FVector(4200, 800, 130), FVector(50, 3600, 260), Concrete);
    Block(World, FVector(2650, 2600, 130), FVector(3100, 50, 260), Concrete);
    auto Gate = [this, World](int32 Id, FVector Position, FRotator Rotation) {
        ABGDoor* Door = World->SpawnActor<ABGDoor>(Position, Rotation);
        if (Door) { Door->EntityId = Id; Doors.Add(Door); }
    };
    Gate(700, FVector(1750, -1000, 125), FRotator::ZeroRotator);
    Gate(701, FVector(1100, 1650, 125), FRotator(0, 90, 0));
    // Roofless laboratory with an actual entrance; walls obstruct both navigation and fire.
    Block(World, FVector(3150, 1500, 10), FVector(1500, 1400, 20), Pavement);
    Block(World, FVector(2400, 1500, 120), FVector(40, 1400, 240), Concrete);
    Block(World, FVector(3900, 1500, 120), FVector(40, 1400, 240), Concrete);
    Block(World, FVector(3150, 2200, 120), FVector(1500, 40, 240), Concrete);
    Block(World, FVector(2580, 800, 120), FVector(360, 40, 240), Concrete);
    Block(World, FVector(3580, 800, 120), FVector(640, 40, 240), Concrete);
    Block(World, FVector(3450, 1650, 65), FVector(380, 100, 130), Steel);
    Block(World, FVector(2900, 1850, 55), FVector(200, 120, 110), Dark);
    // A reachable raised route: continuous sloped decks connect both ends to street level.
    const float RampLength = FMath::Sqrt(1250.f * 1250.f + 300.f * 300.f);
    const float RampPitch = FMath::RadiansToDegrees(FMath::Atan2(300.f, 1250.f));
    Block(World, FVector(-875, 3000, 150), FVector(RampLength, 420, 24), Steel, true, FRotator(RampPitch, 0, 0));
    Block(World, FVector(575, 3000, 300), FVector(1650, 420, 24), Steel);
    Block(World, FVector(2025, 3000, 150), FVector(RampLength, 420, 24), Steel, true, FRotator(-RampPitch, 0, 0));
    Block(World, FVector(ExtractionCenter.X, ExtractionCenter.Y, 15), FVector(1100, 1000, 4), FLinearColor(.15f, .32f, .25f), false);
    for (int32 Index = 0; Index < 4; ++Index)
        Block(World, FVector(-4400 + Index * 380.f, 3200, 35), FVector(60, 60, 70), Mark);
    // Original modular facade and flush street details: no collision, navigation, or save identity.
    FBlackglassDressing Dressing(World);
    Dressing.Facade(FVector(-3750, -650, 270), FVector(1500, 1700, 540));
    Dressing.Facade(FVector(-3700, 650, 200), FVector(1550, 550, 400));
    Dressing.Facade(FVector(-3400, -3300, 210), FVector(2200, 600, 420));
    Dressing.Facade(FVector(1500, -3250, 260), FVector(3100, 650, 520));
    Dressing.Facade(FVector(200, 1250, 280), FVector(1200, 2200, 560));
    const FLinearColor Curb(.51f, .5f, .44f), Grate(.065f, .075f, .08f);
    const float CurbRows[] = { -3350.f, -2750.f, -700.f, 0.f, 700.f, 1400.f, 2950.f, 3500.f };
    for (float Y : CurbRows) for (float X : { -2205.f, -1195.f }) {
        Dressing.Add(TEXT("Curbs"), FVector(X, Y, 25), FVector(14, 530, 6), Curb, .89f);
        Dressing.Add(TEXT("DrainFrames"), FVector(X + (X < -1700 ? 40 : -40), Y + 120, 13),
            FVector(45, 90, 2), Grate, .58f);
        for (int32 Slat = 0; Slat < 4; ++Slat)
            Dressing.Add(TEXT("Curbs"), FVector(X + (X < -1700 ? 40 : -40), Y + 91 + Slat * 19.f, 15),
                FVector(37, 4, 2), Curb, .89f);
    }
    for (int32 Stripe = 0; Stripe < 10; ++Stripe) {
        Dressing.Add(TEXT("CrosswalkPaint"), FVector(-2080 + Stripe * 85.f, -1130, 15),
            FVector(48, 220, 2), Mark, .95f);
        Dressing.Add(TEXT("CrosswalkPaint"), FVector(-1070, -2180 + Stripe * 85.f, 15),
            FVector(220, 48, 2), Mark, .95f);
    }
    Dressing.Add(TEXT("ConcreteTrim"), FVector(1337.5f, -1000, 263), FVector(475, 62, 6), Curb, .88f);
    Dressing.Add(TEXT("ConcreteTrim"), FVector(3062.5f, -1000, 263), FVector(2275, 62, 6), Curb, .88f);
    Dressing.Add(TEXT("ConcreteTrim"), FVector(1100, 237.5f, 263), FVector(62, 2475, 6), Curb, .88f);
    Dressing.Add(TEXT("ConcreteTrim"), FVector(1100, 2212.5f, 263), FVector(62, 775, 6), Curb, .88f);
    Dressing.Add(TEXT("ConcreteTrim"), FVector(4200, 800, 263), FVector(62, 3600, 6), Curb, .88f);
    Dressing.Add(TEXT("ConcreteTrim"), FVector(2650, 2600, 263), FVector(3100, 62, 6), Curb, .88f);
    // Gate apertures and the roofless laboratory entrance remain visibly and physically clear.
    const FLinearColor Identity(.55f, .3f, .12f);
    Dressing.Add(TEXT("CorporateIdentity"), FVector(1460, -1030, 190), FVector(160, 4, 45), Identity, .7f);
    Dressing.Add(TEXT("CorporateIdentity"), FVector(2050, -1030, 190), FVector(160, 4, 45), Identity, .7f);
    Dressing.Add(TEXT("LaboratoryTrim"), FVector(2400, 1500, 244), FVector(48, 1400, 8), Curb, .88f);
    Dressing.Add(TEXT("LaboratoryTrim"), FVector(3900, 1500, 244), FVector(48, 1400, 8), Curb, .88f);
    Dressing.Add(TEXT("LaboratoryTrim"), FVector(3150, 2200, 244), FVector(1500, 48, 8), Curb, .88f);
    Dressing.Add(TEXT("LaboratoryTrim"), FVector(2580, 800, 244), FVector(360, 48, 8), Curb, .88f);
    Dressing.Add(TEXT("LaboratoryTrim"), FVector(3580, 800, 244), FVector(640, 48, 8), Curb, .88f);
    UE_LOG(LogTemp, Log, TEXT("BLACKGLASS original visual dressing: %d instances in %d material batches; collision disabled."),
        Dressing.Instances, Dressing.Batches.Num());
    if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 2000), FRotator(-55, -35, 0))) {
        if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent())) SunLight->SetForwardShadingPriority(1);
        Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Sun->GetLightComponent()->SetCastShadows(true);
        Sun->GetLightComponent()->SetIntensity(3.5f);
        Sun->GetLightComponent()->SetLightColor(FLinearColor(.95f, .9f, .82f));
    }
    // Restrained opposite fill illuminates facades when the captured background is empty.
    if (ADirectionalLight* Fill = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 1800), FRotator(-30, 145, 0))) {
        if (UDirectionalLightComponent* FillLight = Cast<UDirectionalLightComponent>(Fill->GetLightComponent())) FillLight->SetForwardShadingPriority(0);
        Fill->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Fill->GetLightComponent()->SetCastShadows(false);
        Fill->GetLightComponent()->SetIntensity(.65f);
        Fill->GetLightComponent()->SetLightColor(FLinearColor(.65f, .72f, .8f));
    }
    if (ASkyLight* Sky = World->SpawnActor<ASkyLight>()) {
        Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Sky->GetLightComponent()->SetIntensity(.9f);
        Sky->GetLightComponent()->SetLightColor(FLinearColor(.65f, .73f, .82f));
        Sky->GetLightComponent()->bLowerHemisphereIsBlack = true;
        Sky->GetLightComponent()->SetLowerHemisphereColor(FLinearColor(.08f, .1f, .12f));
        Sky->GetLightComponent()->RecaptureSky();
    }
    ABGNavBounds* Bounds = World->SpawnActor<ABGNavBounds>(FVector(0, 0, 350), FRotator::ZeroRotator);
    if (UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)) {
        if (Bounds) Navigation->OnNavigationBoundsUpdated(Bounds);
    }
    FTimerDelegate NavigationBuild;
    NavigationBuild.BindWeakLambda(this, [this]() {
        if (UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld())) Navigation->Build();
    });
    World->GetTimerManager().SetTimerForNextTick(NavigationBuild);
    const TCHAR* OperativeNames[] = { TEXT("VOSS"), TEXT("KLINE"), TEXT("MORROW"), TEXT("KESTREL") };
    for (int32 Index = 0; Index < 4; ++Index)
        SpawnUnit(Index + 1, EBGRole::Operative, OperativeNames[Index], FVector(-3600 + (Index % 2) * 150.f, -2700 + (Index / 2) * 170.f, 125));
    for (int32 Index = 0; Index < 8; ++Index) {
        ABGUnit* Civilian = SpawnUnit(100 + Index, EBGRole::Civilian, FString::Printf(TEXT("District worker %02d"), Index + 1),
            FVector(-2470, -2200 + Index * 500.f, 125));
        if (Civilian) Civilian->PatrolPoints = { FVector(-2470, -2200, 100), FVector(-2470, 1300, 100),
            FVector(-900, 1300, 100), FVector(-900, -2200, 100) };
    }
    const FVector GuardPositions[] = { FVector(1650, -450, 125), FVector(3600, 0, 125), FVector(1800, 1800, 125), FVector(3100, 1100, 125) };
    for (int32 Index = 0; Index < 4; ++Index) {
        ABGUnit* Guard = SpawnUnit(200 + Index, EBGRole::Guard, FString::Printf(TEXT("Facility security %02d"), Index + 1), GuardPositions[Index]);
        if (Guard) Guard->PatrolPoints = { GuardPositions[Index], FVector(1600, 400, 100), FVector(2100, 1800, 100), FVector(3700, 400, 100) };
    }
    Specialist = SpawnUnit(500, EBGRole::Specialist, TEXT("Research specialist: Iona Vale"), FVector(3100, 1650, 125));
    Vehicle = World->SpawnActor<ABGVehicle>(FVector(-2950, -1800, 90), FRotator(0, 90, 0));
    if (Vehicle) { Vehicle->EntityId = 600; Vehicle->Occupants.Init(0, 6); }
}

ABGUnit* ABGOperation::SpawnUnit(int32 Id, EBGRole NewRole, const FString& Label, const FVector& Location) {
    if (ABGUnit* Existing = FindUnit(Id)) return Existing;
    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    ABGUnit* Unit = GetWorld()->SpawnActor<ABGUnit>(Location, FRotator::ZeroRotator, Parameters);
    if (Unit) {
        Unit->Initialize(Id, NewRole, Label);
        Unit->SetActorHiddenInGame(NewRole == EBGRole::Guard);
        Units.Add(Unit);
    }
    return Unit;
}

ABGUnit* ABGOperation::FindUnit(int32 Id) const {
    for (ABGUnit* Unit : Units) if (IsValid(Unit) && Unit->EntityId == Id) return Unit;
    return nullptr;
}

AActor* ABGOperation::FindEntity(int32 Id) const {
    if (ABGUnit* Unit = FindUnit(Id)) return Unit;
    if (IsValid(Vehicle) && Vehicle->EntityId == Id) return Vehicle;
    for (ABGDoor* Door : Doors) if (IsValid(Door) && Door->EntityId == Id) return Door;
    return nullptr;
}

bool ABGOperation::CanSee(const ABGUnit* Observer, const ABGUnit* Target) const {
    if (!IsValid(Observer) || !IsValid(Target) || !Observer->Alive()) return false;
    const float Range = Observer->UnitRole == EBGRole::Operative ? 2300.f : Observer->UnitRole == EBGRole::Guard ? 1800.f : 1000.f;
    if (FVector::DistSquared(Observer->EffectiveLocation(), Target->EffectiveLocation()) > FMath::Square(Range)) return false;
    FCollisionQueryParams Parameters(SCENE_QUERY_STAT(BlackglassSight), false);
    Parameters.AddIgnoredActor(Observer);
    if (Observer->IsSeated() && IsValid(Vehicle)) Parameters.AddIgnoredActor(Vehicle);
    FHitResult Hit;
    const FVector Start = Observer->EffectiveLocation() + FVector(0, 0, 45);
    const FVector End = Target->EffectiveLocation() + FVector(0, 0, 30);
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Parameters)) return true;
    return Hit.GetActor() == Target || (Target->IsSeated() && Hit.GetActor() == Vehicle);
}

bool ABGOperation::IsDetected(const ABGUnit* Target) const {
    if (!IsValid(Target)) return false;
    if (Target->UnitRole != EBGRole::Guard) return true;
    for (const ABGUnit* Unit : Units)
        if (IsValid(Unit) && Unit->UnitRole == EBGRole::Operative && Unit->Alive() && CanSee(Unit, Target)) return true;
    return false;
}

void ABGOperation::ReportIncident(const FVector& Location, int32 SourceId, float Radius) {
    if (bRestoring || Outcome != EBGOutcome::Active || !FMath::IsFinite(Radius) || Radius <= 0) return;
    ABGUnit* Source = FindUnit(SourceId);
    bool GuardHeard = false;
    for (ABGUnit* Unit : Units) {
        if (!IsValid(Unit) || !Unit->Alive() || Unit->EntityId == SourceId
            || FVector::DistSquared(Unit->EffectiveLocation(), Location) > FMath::Square(Radius)) continue;
        const int32 ConfirmedSource = Source && CanSee(Unit, Source) ? SourceId : 0;
        Unit->HearIncident(Location, ConfirmedSource);
        GuardHeard |= Unit->UnitRole == EBGRole::Guard;
    }
    if (GuardHeard && (!Source || Source->UnitRole == EBGRole::Operative)) {
        const bool NewAlarm = AlarmSeconds <= 0;
        AlarmSeconds = FMath::Max(AlarmSeconds, 65.f);
        ReportedPosition = Location;
        if (NewAlarm) Notify(TEXT("Security alarm: local gunfire report. Response teams investigate the reported position."));
    }
}

void ABGOperation::AcquireSpecialist(ABGUnit* Controller) {
    if (bRestoring || Outcome != EBGOutcome::Active || !IsValid(Controller) || !Controller->Alive()
        || Controller->UnitRole != EBGRole::Operative || !IsValid(Specialist) || !Specialist->Alive()) return;
    if (Controller->IsSeated() || Specialist->IsSeated()
        || FVector::DistSquared2D(Controller->EffectiveLocation(), Specialist->EffectiveLocation()) > FMath::Square(350.f)
        || !CanSee(Controller, Specialist)) { Notify(TEXT("Specialist interaction requires a nearby operative with a clear line of sight.")); return; }
    bSpecialistAcquired = true; SpecialistLeaderId = Controller->EntityId;
    Specialist->StopOrders(false);
    Notify(TEXT("Specialist secured as an escort. Keep the operative alive and extract together."));
}

void ABGOperation::Tick(float DeltaSeconds) {
    Super::Tick(DeltaSeconds);
    if (bRestoring || Outcome != EBGOutcome::Active) return;
    const float PreviousTime = SimulationSeconds;
    SimulationSeconds += DeltaSeconds;
    AlarmSeconds = FMath::Max(0.f, AlarmSeconds - DeltaSeconds);
    if (AlarmSeconds > 0 && Reinforcements < 4 && FMath::FloorToInt(SimulationSeconds / 12.f) > FMath::FloorToInt(PreviousTime / 12.f)) {
        ABGUnit* Response = SpawnUnit(204 + Reinforcements, EBGRole::Guard, TEXT("Corporate response"),
            FVector(3200 + Reinforcements * 130.f, 3350, 125));
        if (Response) {
            Response->LastKnown = ReportedPosition; Response->EvidenceSeconds = 20; Response->ThreatId = 0;
            FBGOrder Investigation; Investigation.Type = EBGOrderType::Move; Investigation.Location = ReportedPosition;
            Response->IssueOrder(Investigation, false);
            ++Reinforcements;
        }
    }
    for (ABGUnit* Unit : Units) if (IsValid(Unit) && Unit->UnitRole == EBGRole::Guard) Unit->SetActorHiddenInGame(!IsDetected(Unit));
    if (bSpecialistAcquired && IsValid(Specialist) && Specialist->Alive()) {
        ABGUnit* Leader = FindUnit(SpecialistLeaderId);
        if (!IsValid(Leader) || !Leader->Alive()) {
            if (SpecialistLeaderId != 0) { SpecialistLeaderId = 0; Specialist->StopOrders(); Notify(TEXT("Escort leader lost. Another operative must approach and interact to resume extraction.")); }
        } else if (Leader->IsSeated() && IsValid(Vehicle) && Vehicle->Health > 0) {
            if (!Specialist->IsSeated()) {
                if (FVector::DistSquared2D(Specialist->EffectiveLocation(), Vehicle->GetActorLocation()) <= FMath::Square(300.f)) Vehicle->Board(Specialist);
                else if (!Specialist->bHasOrder || Specialist->Order.Type != EBGOrderType::Board || Specialist->Order.TargetId != Vehicle->EntityId) {
                    FBGOrder Boarding; Boarding.Type = EBGOrderType::Board; Boarding.TargetId = Vehicle->EntityId;
                    Specialist->IssueOrder(Boarding, false);
                }
            }
        } else if (Specialist->IsSeated()) {
            if (IsValid(Vehicle)) Vehicle->Exit(Specialist);
        } else if (FVector::DistSquared2D(Specialist->EffectiveLocation(), Leader->EffectiveLocation()) > FMath::Square(250.f)
            && (!Specialist->bHasOrder || Specialist->Order.Type != EBGOrderType::Move
                || FVector::DistSquared2D(Specialist->Order.Location, Leader->EffectiveLocation() + FVector(-80, -80, 0)) > FMath::Square(180.f))) {
            FBGOrder Follow; Follow.Location = Leader->EffectiveLocation() + FVector(-80, -80, 0); Specialist->IssueOrder(Follow, false);
        }
    }
    EvaluateMission();
    if (Outcome == EBGOutcome::Active && FMath::FloorToInt(SimulationSeconds / 120.f) > FMath::FloorToInt(PreviousTime / 120.f)) SaveOperation(TEXT("autosave"));
}

void ABGOperation::EvaluateMission() {
    if (bRestoring || Outcome != EBGOutcome::Active) return;
    if (!IsValid(Specialist) || !Specialist->Alive()) { Fail(TEXT("Research specialist lost. Acquisition contract failed.")); return; }
    int32 Survivors = 0, AtExtraction = 0;
    for (const ABGUnit* Unit : Units) if (IsValid(Unit) && Unit->UnitRole == EBGRole::Operative && Unit->Alive()) {
        ++Survivors;
        if (InsideExtraction(this, Unit->EffectiveLocation())) ++AtExtraction;
    }
    if (Survivors == 0) { Fail(TEXT("All deployed operatives lost. The acquisition operation has failed.")); return; }
    if (bSpecialistAcquired && Survivors > 0 && AtExtraction == Survivors && InsideExtraction(this, Specialist->EffectiveLocation())) {
        Outcome = EBGOutcome::Success;
        if (!bRewardSettled) { Credits += 6000; bRewardSettled = true; }
        for (ABGUnit* Unit : Units) if (IsValid(Unit)) Unit->StopOrders(true);
        if (IsValid(Vehicle)) Vehicle->StopTravel();
        Notify(TEXT("Acquisition complete. Specialist and all surviving operatives extracted. Contract revenue: 6000 credits. Restart for another run."));
        SaveOperation(TEXT("autosave"));
    }
}

void ABGOperation::Fail(const FString& Reason) {
    if (bRestoring || Outcome != EBGOutcome::Active) return;
    Outcome = EBGOutcome::Failed;
    for (ABGUnit* Unit : Units) if (IsValid(Unit)) Unit->StopOrders(true);
    if (IsValid(Vehicle)) Vehicle->StopTravel();
    Notify(Reason);
    SaveOperation(TEXT("autosave"));
}

void ABGOperation::Notify(const FString& Message) {
    if (Notice == Message && NoticeExpires > SimulationSeconds) return;
    Notice = Message; NoticeExpires = SimulationSeconds + 7.f;
    UE_LOG(LogTemp, Display, TEXT("BLACKGLASS: %s"), *Message);
}

void ABGOperation::RestartOperation() {
    UGameplayStatics::SetGamePaused(this, false);
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}
