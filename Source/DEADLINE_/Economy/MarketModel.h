// Copyright DEADLINE. All Rights Reserved.
//
// The GDD 8.1 price model as pure functions, with no UObject and no world.
//
// It lives apart from UMarketSubsystem for one reason: the automation test
// Deadline.Economy.MarketParity has to run this arithmetic outside a running
// game and diff it against Tests/MarketParityGolden.csv, the series produced
// by EconomyPrototype. If the model were welded into the subsystem, the only
// way to check it would be to launch the game and read prices off the HUD.
//
// Every line here has a counterpart in EconomyPrototype/market.py and
// EconomyPrototype/rng.py. When you change one, change the other and
// regenerate the golden file, or the parity test will tell you off.

#pragma once

#include "CoreMinimal.h"
#include "Data/ProductRow.h"
#include "Math/RandomStream.h"

/**
 * Deterministic seed derivation. Mirrors EconomyPrototype/rng.py.
 *
 * Deliberately not GetTypeHash(FName): that depends on the engine's name table
 * and therefore on load order, so it would give a different market every time
 * assets happened to load in a different sequence.
 */
namespace DeadlineSeed
{
	/** FNV-1a over the UTF-8 bytes of Text. */
	DEADLINE__API uint32 FNV1a32(const FString& Text);

	/** Sub-seed for a named slice of a run seed. */
	DEADLINE__API int32 Derive(int32 BaseSeed, const FString& Key);
}

/**
 * GDD 8.1:
 *
 *     NewPrice = OldPrice
 *              + Pull * (NormalPrice - OldPrice)   <- mean reversion
 *              + EventImpulse                       <- events
 *              + Noise                              <- small randomness
 *              + PlayerPressure                     <- your market share
 *     clamped to the risk band's hard floor / ceiling.
 *
 * All arithmetic is double. The band constants are doubles for the same
 * reason (see FRiskBandParams): float rounding compounds over 200 days and
 * would drift away from the Python reference.
 */
struct DEADLINE__API FMarketModel
{
	/** Stream names from CLAUDE.md. Part of the seed derivation, so the
	    spelling matters -- EconomyPrototype/market.py uses the same strings. */
	static const TCHAR* MarketStreamKey;

	/**
	 * The stream for one product.
	 *
	 * Per-product streams are not a style choice. They are what makes price a
	 * pure function of (seed, product, day). With one shared stream, working
	 * out today's price for a single product would have to consume the draws
	 * owed to the other 62 products, so the answer would depend on which
	 * products the player had looked at. Lazy evaluation (CLAUDE.md) needs
	 * this property.
	 */
	static FRandomStream MakeProductStream(int32 BaseSeed, FName ProductID);

	/** FRandRange in double: Min + (Max - Min) * GetFraction(). */
	static double Uniform(FRandomStream& Stream, double Min, double Max)
	{
		return Min + (Max - Min) * static_cast<double>(Stream.GetFraction());
	}

	/**
	 * One simulated day for one product.
	 *
	 * @param EventImpulse   Fraction of BasePrice, summed over active events.
	 *        Always 0 in Month 2: the seeded event calendar is UEventSubsystem
	 *        and the roadmap puts it in Month 4. The term is in the signature
	 *        because it is in the GDD formula, not as a guess at what Month 4
	 *        will want.
	 * @param PlayerPressure Absolute dollars, from hoarding a big share of a
	 *        product (GDD 7.8). Always 0 until Month 7, same reasoning.
	 */
	static double AdvanceOneDay(
		double OldPrice,
		double NormalPrice,
		double BasePrice,
		ERiskBand Band,
		double EventImpulse,
		double PlayerPressure,
		FRandomStream& Stream);

	/** Hard band clamp (GDD 8.1). The last thing that happens every day. */
	static double ClampToBand(double Price, double BasePrice, ERiskBand Band);

	/**
	 * Simulate a product from day 0 to Days, from scratch.
	 *
	 * Used by the parity test and the Dl_Market* cheats. The subsystem does
	 * not call this: it advances incrementally instead, and lands on the same
	 * numbers because the stream is per-product.
	 *
	 * @return Days + 1 prices, index 0 being BasePrice on day 0.
	 */
	static TArray<double> SimulateSeries(int32 BaseSeed, FName ProductID,
		double BasePrice, ERiskBand Band, int32 Days);
};
