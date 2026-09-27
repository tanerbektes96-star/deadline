// Copyright DEADLINE. All Rights Reserved.

#include "Economy/MarketModel.h"

const TCHAR* FMarketModel::MarketStreamKey = TEXT("MarketStream");

namespace DeadlineSeed
{
	uint32 FNV1a32(const FString& Text)
	{
		const FTCHARToUTF8 Utf8(*Text);
		const uint8* Bytes = reinterpret_cast<const uint8*>(Utf8.Get());
		const int32 Length = Utf8.Length();

		uint32 Hash = 0x811C9DC5u;
		for (int32 Index = 0; Index < Length; ++Index)
		{
			Hash = (Hash ^ Bytes[Index]) * 0x01000193u;
		}
		return Hash;
	}

	int32 Derive(int32 BaseSeed, const FString& Key)
	{
		uint32 Hash = FNV1a32(Key);
		Hash ^= static_cast<uint32>(BaseSeed);
		Hash *= 0x01000193u;
		return static_cast<int32>(Hash);
	}
}

FRandomStream FMarketModel::MakeProductStream(int32 BaseSeed, FName ProductID)
{
	const FString Key = FString::Printf(TEXT("%s:%s"), MarketStreamKey, *ProductID.ToString());
	return FRandomStream(DeadlineSeed::Derive(BaseSeed, Key));
}

double FMarketModel::ClampToBand(double Price, double BasePrice, ERiskBand Band)
{
	const FRiskBandParams Params = FProductRow::GetRiskBandParams(Band);
	const double Floor = BasePrice * Params.FloorMultiplier;
	const double Ceiling = BasePrice * Params.CeilingMultiplier;
	return FMath::Clamp(Price, Floor, Ceiling);
}

double FMarketModel::AdvanceOneDay(
	double OldPrice,
	double NormalPrice,
	double BasePrice,
	ERiskBand Band,
	double EventImpulse,
	double PlayerPressure,
	FRandomStream& Stream)
{
	const FRiskBandParams Params = FProductRow::GetRiskBandParams(Band);

	const double PullTerm  = Params.MeanReversionPull * (NormalPrice - OldPrice);
	const double EventTerm = EventImpulse * BasePrice;
	const double NoiseTerm = Uniform(Stream, -Params.Volatility, Params.Volatility) * NormalPrice;

	// The draw above happens for every product on every day, whether or not
	// anything reads the result. That is what keeps a lazily-advanced product
	// in step with one the player has been watching all along.
	const double NewPrice = OldPrice + PullTerm + EventTerm + NoiseTerm + PlayerPressure;

	return ClampToBand(NewPrice, BasePrice, Band);
}

TArray<double> FMarketModel::SimulateSeries(int32 BaseSeed, FName ProductID,
	double BasePrice, ERiskBand Band, int32 Days)
{
	TArray<double> Series;
	if (Days < 0)
	{
		return Series;
	}

	Series.Reserve(Days + 1);
	Series.Add(BasePrice);

	FRandomStream Stream = MakeProductStream(BaseSeed, ProductID);
	double Price = BasePrice;
	for (int32 Day = 1; Day <= Days; ++Day)
	{
		Price = AdvanceOneDay(Price, /*NormalPrice=*/BasePrice, BasePrice, Band,
			/*EventImpulse=*/0.0, /*PlayerPressure=*/0.0, Stream);
		Series.Add(Price);
	}
	return Series;
}
