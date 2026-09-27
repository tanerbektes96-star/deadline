// Copyright DEADLINE. All Rights Reserved.
//
// GDD 11 — the warehouse is not one big number of Box Units. It is four
// storage classes, each with its own per-unit capacity and its own idea of
// what it will accept:
//
//   Rack         12 BU per rack    Box S / M / L
//   Pallet bay   16 BU per bay     Pallet, Crate
//   Cold zone    24 BU per zone    Cold Tote        (draws power, Month 5)
//   Secure rack   8 BU per cage    Secure Case      (theft cover, Month 7)
//
// The container type decides the class, so nothing has to remember where a
// box was put: a Secure Case is always in a cage, a Cold Tote is always in the
// cold zone. That is what makes "which shelf is full?" answerable without
// storing a location per box.
//
// Not here yet, on purpose:
//   Floor storage  — GDD 11 lists it as unlimited-but-penalised, and the
//                    penalties (slow walking, inspection minus) are Month 7.
//   Safe room      — an upgrade whose whole point is hiding stock from a
//                    routine inspection, which is also Month 7.

#pragma once

#include "CoreMinimal.h"
#include "Data/ProductRow.h"
#include "StorageClass.generated.h"

UENUM(BlueprintType)
enum class EStorageClass : uint8
{
	Rack        UMETA(DisplayName = "Rack"),
	PalletBay   UMETA(DisplayName = "Pallet Bay"),
	ColdZone    UMETA(DisplayName = "Cold Zone"),
	SecureRack  UMETA(DisplayName = "Secure Rack"),
};

/** How many entries EStorageClass has. Used to size per-class arrays. */
static constexpr int32 NumStorageClasses = 4;

/** The GDD 11 table, as code. Single source of truth for storage rules. */
struct FStorageClassRules
{
	/** Box Units one rack / bay / zone / cage holds (GDD 11). */
	static float BUPerUnit(EStorageClass Class)
	{
		switch (Class)
		{
		case EStorageClass::Rack:       return 12.f;
		case EStorageClass::PalletBay:  return 16.f;
		case EStorageClass::ColdZone:   return 24.f;
		case EStorageClass::SecureRack: return 8.f;
		}
		return 0.f;
	}

	/** Which class a container belongs in. Every container type has exactly
	    one home, which is what keeps placement implicit. */
	static EStorageClass ClassForContainer(EContainerType Type)
	{
		switch (Type)
		{
		case EContainerType::BoxS:
		case EContainerType::BoxM:
		case EContainerType::BoxL:       return EStorageClass::Rack;
		case EContainerType::Pallet:
		case EContainerType::Crate:      return EStorageClass::PalletBay;
		case EContainerType::ColdTote:   return EStorageClass::ColdZone;
		case EContainerType::SecureCase: return EStorageClass::SecureRack;
		}
		return EStorageClass::Rack;
	}

	static bool Accepts(EStorageClass Class, EContainerType Type)
	{
		return ClassForContainer(Type) == Class;
	}

	static FText DisplayName(EStorageClass Class)
	{
		switch (Class)
		{
		case EStorageClass::Rack:       return NSLOCTEXT("Deadline", "StorageRack", "Rack");
		case EStorageClass::PalletBay:  return NSLOCTEXT("Deadline", "StorageBay", "Pallet bay");
		case EStorageClass::ColdZone:   return NSLOCTEXT("Deadline", "StorageCold", "Cold zone");
		case EStorageClass::SecureRack: return NSLOCTEXT("Deadline", "StorageSecure", "Secure rack");
		}
		return FText::GetEmpty();
	}

	/** Iteration helper so no loop has to hardcode the four values. */
	static EStorageClass FromIndex(int32 Index)
	{
		return static_cast<EStorageClass>(FMath::Clamp(Index, 0, NumStorageClasses - 1));
	}
};
