// Fill out your copyright notice in the Description page of Project Settings.
#include "Character/AAADPlayerState.h"
#include "Character/ALSPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Character/ALSBaseCharacter.h"
#include "Net/OnlineEngineInterface.h"

//TODO: Score points function, create game state, create kill functions
AAAADPlayerState::AAAADPlayerState(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	TeamNumber = 0;
	NumKills = 0;
	NumDeaths = 0;
	NumBulletsFired = 0;
	NumRocketsFired = 0;
	bQuitter = false;
}

void AAAADPlayerState::Reset()
{
	Super::Reset();

	//PlayerStates persist across seamless travel.  Keep the same teams as previous match.
	//SetTeamNum(0);
	NumKills = 0;
	NumDeaths = 0;
	NumBulletsFired = 0;
	NumRocketsFired = 0;
	bQuitter = false;
}

void AAAADPlayerState::RegisterPlayerWithSession(bool bWasFromInvite)
{
	if (UOnlineEngineInterface::Get()->DoesSessionExist(GetWorld(), NAME_GameSession))
	{
		Super::RegisterPlayerWithSession(bWasFromInvite);
	}
}

void AAAADPlayerState::UnregisterPlayerWithSession()
{
	if (!IsFromPreviousLevel() && UOnlineEngineInterface::Get()->DoesSessionExist(GetWorld(), NAME_GameSession))
	{
		Super::UnregisterPlayerWithSession();
	}
}

void AAAADPlayerState::ClientInitialize(AController* InController)
{
	Super::ClientInitialize(InController);

	UpdateTeamColors();
}

void AAAADPlayerState::SetTeamNum(int32 NewTeamNumber)
{
	TeamNumber = NewTeamNumber;

	UpdateTeamColors();
}

void AAAADPlayerState::OnRep_TeamColor()
{
	UpdateTeamColors();
}

void AAAADPlayerState::AddBulletsFired(int32 NumBullets)
{
	NumBulletsFired += NumBullets;
}

void AAAADPlayerState::AddRocketsFired(int32 NumRockets)
{
	NumRocketsFired += NumRockets;
}

void AAAADPlayerState::SetQuitter(bool bInQuitter)
{
	bQuitter = bInQuitter;
}

void AAAADPlayerState::SetMatchId(const FString& CurrentMatchId)
{
	MatchId = CurrentMatchId;
}

void AAAADPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	AAAADPlayerState* ShooterPlayer = Cast<AAAADPlayerState>(PlayerState);
	if (ShooterPlayer)
	{
		ShooterPlayer->TeamNumber = TeamNumber;
	}
}

void AAAADPlayerState::UpdateTeamColors()
{
	AController* OwnerController = Cast<AController>(GetOwner());
	if (OwnerController != NULL)
	{
		AALSBaseCharacter* ALSBaseCharacter = Cast<AALSBaseCharacter>(OwnerController->GetCharacter());
		if (ALSBaseCharacter != NULL)
		{
			//ALSBaseCharacter->UpdateTeamColorsAllMIDs();
		}
	}
}

int32 AAAADPlayerState::GetTeamNum() const
{
	return TeamNumber;
}

int32 AAAADPlayerState::GetKills() const
{
	return NumKills;
}

int32 AAAADPlayerState::GetDeaths() const
{
	return NumDeaths;
}

int32 AAAADPlayerState::GetNumBulletsFired() const
{
	return NumBulletsFired;
}

int32 AAAADPlayerState::GetNumRocketsFired() const
{
	return NumRocketsFired;
}

bool AAAADPlayerState::IsQuitter() const
{
	return bQuitter;
}

FString AAAADPlayerState::GetMatchId() const
{
	return MatchId;
}

void AAAADPlayerState::ScoreKill(AAAADPlayerState* Victim, int32 Points)
{
	NumKills++;
	ScorePoints(Points);
}

void AAAADPlayerState::ScoreDeath(AAAADPlayerState* KilledBy, int32 Points)
{
	NumDeaths++;
	ScorePoints(Points);
}

void AAAADPlayerState::ScorePoints(int32 Points)
{
/**
	AShooterGameState* const MyGameState = GetWorld()->GetGameState<AShooterGameState>();
	if (MyGameState && TeamNumber >= 0)
	{
		if (TeamNumber >= MyGameState->TeamScores.Num())
		{
			MyGameState->TeamScores.AddZeroed(TeamNumber - MyGameState->TeamScores.Num() + 1);
		}

		MyGameState->TeamScores[TeamNumber] += Points;
	}

	SetScore(GetScore() + Points);
	**/
}

void AAAADPlayerState::InformAboutKill_Implementation(class AAAADPlayerState* KillerPlayerState, const UDamageType* KillerDamageType, class AAAADPlayerState* KilledPlayerState)
{
	//id can be null for bots
	
	if (KillerPlayerState->GetUniqueId().IsValid())
	{
		//search for the actual killer before calling OnKill()	
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			AALSPlayerController* TestPC = Cast<AALSPlayerController>(*It);
			if (TestPC && TestPC->IsLocalController())
			{
				// a local player might not have an ID if it was created with CreateDebugPlayer.
				ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(TestPC->Player);
				FUniqueNetIdRepl LocalID = LocalPlayer->GetCachedUniqueNetId();
				if (LocalID.IsValid() && *LocalPlayer->GetCachedUniqueNetId() == *KillerPlayerState->GetUniqueId())
				{
					//TestPC->OnKill();
				}
			}
		}
	}
	
}

void AAAADPlayerState::BroadcastDeath_Implementation(class AAAADPlayerState* KillerPlayerState, const UDamageType* KillerDamageType, class AAAADPlayerState* KilledPlayerState)
{

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		// all local players get death messages so they can update their huds.
		AALSPlayerController* TestPC = Cast<AALSPlayerController>(*It);
		if (TestPC && TestPC->IsLocalController())
		{
			//TestPC->OnDeathMessage(KillerPlayerState, this, KillerDamageType);
		}
	}
	
}

void AAAADPlayerState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAAADPlayerState, TeamNumber);
	DOREPLIFETIME(AAAADPlayerState, NumKills);
	DOREPLIFETIME(AAAADPlayerState, NumDeaths);
}

FString AAAADPlayerState::GetShortPlayerName() const
{

	//if (GetPlayerName().Len() > MAX_PLAYER_NAME_LENGTH)
	//{
		//return GetPlayerName().Left(MAX_PLAYER_NAME_LENGTH) + "...";
	//}
	return GetPlayerName();
}