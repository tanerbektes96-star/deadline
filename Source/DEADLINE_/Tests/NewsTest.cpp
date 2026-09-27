// Copyright DEADLINE. All Rights Reserved.
//
// The news layer (GDD 16). The calendar tests prove the events are fair; these
// prove the player hears about them, at the right hour, and hears nothing
// they should not: a rumour must read the same whether or not it comes true.
//
//   "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" \
//     "C:\Users\PC\Desktop\DEADLINE_\DEADLINE_.uproject" \
//     -ExecCmds="Automation RunTests Deadline.News;Quit" \
//     -unattended -nopause -nosplash -nullrhi -log

#include "CoreMinimal.h"
#include "Core/DeadlineLocale.h"
#include "Core/DeadlineSettings.h"
#include "Core/SaveSubsystem.h"
#include "Core/TimeSubsystem.h"
#include "Data/EventRow.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Events/EventSubsystem.h"
#include "Events/NewsSubsystem.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UGameInstance* MakeNewsInstance(int32 Seed)
	{
		UGameInstance* GI = NewObject<UGameInstance>(GEngine);
		if (GI)
		{
			GI->InitializeStandalone();
			if (USaveSubsystem* Save = GI->GetSubsystem<USaveSubsystem>())
			{
				Save->StartNewGame(Seed);
			}
		}
		return GI;
	}

	const FNewsItem* FindItem(const FNewsBulletin& B, ENewsKind Kind, FName EventID)
	{
		return B.Items.FindByPredicate([Kind, EventID](const FNewsItem& I) { return I.Kind == Kind && I.EventID == EventID; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineNewsBulletinTest,
	"Deadline.News.Bulletins",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineNewsBulletinTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = MakeNewsInstance(31337);
	UEventSubsystem* Events = GI ? GI->GetSubsystem<UEventSubsystem>() : nullptr;
	UNewsSubsystem* News = GI ? GI->GetSubsystem<UNewsSubsystem>() : nullptr;
	const UDataTable* Table = UDeadlineSettings::Get().EventTable.LoadSynchronous();
	if (!Events || !News || !Table)
	{
		AddError(TEXT("Missing subsystems or DT_Events"));
		return false;
	}

	constexpr int32 Horizon = 60;
	const TArray<FScheduledEvent> Calendar = Events->GetCalendar(0, Horizon);
	TestTrue(TEXT("The calendar has something to report"), Calendar.Num() >= 8);

	int32 Rumours = 0, Headlines = 0, Denials = 0, Endings = 0, Updates = 0;
	for (const FScheduledEvent& E : Calendar)
	{
		const FString ID = E.EventID.ToString();
		const FEventRow* Row = Table->FindRow<FEventRow>(E.EventID, TEXT("test"), false);

		// The rumour, in the morning bulletin of its day.
		// Keep the bulletin alive: FindItem returns a pointer into it.
		const FNewsBulletin Morning = News->BuildBulletin(E.SignalDay, UNewsSubsystem::MorningHour);
		const FNewsItem* Rumour = FindItem(Morning, ENewsKind::Rumour, E.EventID);
		if (TestNotNull(*FString::Printf(TEXT("%s day %d: rumour in the 08:00 bulletin"), *ID, E.SignalDay), Rumour))
		{
			++Rumours;
			// Every field is one the player may see, and none comes from the answer.
			TestEqual(*FString::Printf(TEXT("%s rumour shows the calendar's confidence"), *ID), Rumour->Confidence, E.Confidence);
			TestEqual(*FString::Printf(TEXT("%s rumour names the expected day"), *ID), Rumour->RelatedDay, E.StartDay);
			if (Row)
			{
				TestEqual(*FString::Printf(TEXT("%s rumour text is the table's signal, true or false"), *ID),
					Rumour->Text, DeadlineLocale::Pick(Row->SignalTR, Row->SignalEN));
			}
		}

		if (E.StartDay > Horizon)
		{
			continue;
		}
		const FNewsBulletin Due = News->BuildBulletin(E.StartDay, UNewsSubsystem::MorningHour);
		if (E.bHappens)
		{
			Headlines += TestNotNull(*FString::Printf(TEXT("%s day %d: headline when it starts"), *ID, E.StartDay),
				FindItem(Due, ENewsKind::Headline, E.EventID)) ? 1 : 0;
			TestNull(*FString::Printf(TEXT("%s: not denied when it happened"), *ID), FindItem(Due, ENewsKind::Denied, E.EventID));

			if (E.EndDay <= Horizon)
			{
				Endings += TestNotNull(*FString::Printf(TEXT("%s day %d: announced over"), *ID, E.EndDay),
					FindItem(News->BuildBulletin(E.EndDay, UNewsSubsystem::MorningHour), ENewsKind::Ended, E.EventID)) ? 1 : 0;
			}

			// Midday on its first day: a price update if it pushes prices or routes.
			const bool bMatters = (Row && !Row->Tags.IsEmpty()) || (Row && Row->RouteCostMultiplier > 1.f);
			if (bMatters)
			{
				Updates += TestNotNull(*FString::Printf(TEXT("%s day %d: midday update"), *ID, E.StartDay),
					FindItem(News->BuildBulletin(E.StartDay, UNewsSubsystem::MiddayHour), ENewsKind::Update, E.EventID)) ? 1 : 0;
			}
		}
		else
		{
			Denials += TestNotNull(*FString::Printf(TEXT("%s day %d: false rumour denied"), *ID, E.StartDay),
				FindItem(Due, ENewsKind::Denied, E.EventID)) ? 1 : 0;
			TestNull(*FString::Printf(TEXT("%s: no headline for a false rumour"), *ID), FindItem(Due, ENewsKind::Headline, E.EventID));
		}
	}
	AddInfo(FString::Printf(TEXT("60 days: %d rumours, %d headlines, %d denials, %d endings, %d updates"),
		Rumours, Headlines, Denials, Endings, Updates));
	TestTrue(TEXT("Both outcomes occur in 60 days"), Headlines > 0 && Denials > 0);

	GI->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineNewsLogTest,
	"Deadline.News.Log",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineNewsLogTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = MakeNewsInstance(31337);
	UTimeSubsystem* Time = GI ? GI->GetSubsystem<UTimeSubsystem>() : nullptr;
	UNewsSubsystem* News = GI ? GI->GetSubsystem<UNewsSubsystem>() : nullptr;
	if (!Time || !News)
	{
		AddError(TEXT("Missing subsystems"));
		return false;
	}

	// Day 0, 07:59: nothing is out yet.
	Time->AdvanceMinutes(7 * 60 + 59);
	TestEqual(TEXT("Nothing before the first 08:00"), News->GetNewsLog(7).Num(), 0);

	// Day 9, 10:00: every non-empty bulletin up to day 9 08:00 is in the log,
	// and day 9's 12:00 is not.
	Time->AdvanceMinutes(9 * 24 * 60 + 2 * 60 + 1);
	TestEqual(TEXT("Clock at day 9"), Time->GetDay(), 9);
	const TArray<FNewsBulletin> Log = News->GetNewsLog(10);

	int32 Expected = 0;
	for (int32 Day = 0; Day <= 9; ++Day)
	{
		Expected += News->BuildBulletin(Day, UNewsSubsystem::MorningHour).Items.Num() > 0 ? 1 : 0;
		if (Day < 9)
		{
			Expected += News->BuildBulletin(Day, UNewsSubsystem::MiddayHour).Items.Num() > 0 ? 1 : 0;
		}
	}
	TestEqual(TEXT("Log holds exactly the bulletins already out"), Log.Num(), Expected);
	TestFalse(TEXT("Today's 12:00 is not out at 10:00"),
		Log.ContainsByPredicate([](const FNewsBulletin& B) { return B.Day == 9 && B.Hour == UNewsSubsystem::MiddayHour; }));

	bool bOrdered = true;
	for (int32 i = 1; i < Log.Num(); ++i)
	{
		bOrdered &= (Log[i - 1].Day * 24 + Log[i - 1].Hour) > (Log[i].Day * 24 + Log[i].Hour);
	}
	TestTrue(TEXT("Newest first"), bOrdered);
	TestTrue(TEXT("Latest bulletin is the log's first"),
		Log.Num() == 0 || (News->GetLatestBulletin().Day == Log[0].Day && News->GetLatestBulletin().Hour == Log[0].Hour));

	GI->Shutdown();
	return true;
}

#endif
