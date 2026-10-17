#include "BDHUD.h"
#include "BDBomb.h"
#include "BDGameMode.h"
#include "BDPuzzleActors.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

void ABDHUD::Label(const FString& Text, const FVector& Location, const FLinearColor& Color)
{
    FVector2D Screen;
    if (GetOwningPlayerController()->ProjectWorldLocationToScreen(Location, Screen))
    {
        float W,H;
        const float Scale = FMath::Clamp(Canvas->SizeY / 900.f, .6f, 1.f);
        GetTextSize(Text,W,H,GEngine->GetSmallFont(),Scale);
        DrawText(Text,Color,Screen.X-W*.5f,Screen.Y,GEngine->GetSmallFont(),Scale);
    }
}

void ABDHUD::DrawHUD()
{
    Super::DrawHUD();
    const auto* Mode = GetWorld()->GetAuthGameMode<ABDGameMode>();
    if (!Canvas || !Mode) return;
    const float S = FMath::Clamp(Canvas->SizeY / 800.f, .65f, 1.5f);
    const FLinearColor Ink(.91f,.95f,.94f);
    const FLinearColor Muted(.59f,.7f,.71f);
    const FLinearColor Cyan(.18f,.91f,.93f);
    const FLinearColor Panel(.025f,.06f,.075f,.94f);
    auto Text = [&](const FString& T,float X,float Y,float Scale,const FLinearColor& C)
    { DrawText(T,C,X*S,Y*S,GEngine->GetMediumFont(),Scale*S); };
    DrawRect(Panel,16*S,16*S,262*S,113*S);
    Text(TEXT("BLAST DELIVERY"),30,27,1.1f,Cyan);
    Text(TEXT("TUTORIAL  /  First delivery"),30,55,.68f,Muted);
    const int32 Seconds = FMath::FloorToInt(Mode->ElapsedSeconds);
    Text(FString::Printf(TEXT("BOMBS  %02d      TIME  %02d:%02d"),Mode->BombsUsed,Seconds/60,Seconds%60),30,88,.78f,Ink);

    const bool bWide = float(Canvas->SizeX)/Canvas->SizeY > 1.5f;
    if (bWide)
    {
        DrawRect(Panel,16*S,150*S,262*S,405*S);
        Text(TEXT("YOUR DELIVERY ROUTE"),30,165,.77f,Cyan);
        const bool Open = Mode->Gate && Mode->Gate->bOpen;
        Text(Open ? TEXT("Gate open! Return to the crate.") : TEXT("1  Cross the lower pit."),30,202,.70f,Ink);
        Text(Open ? TEXT("Blast it up, then right through") : TEXT("Face LEFT on the launch pad."),30,229,.67f,Ink);
        Text(Open ? TEXT("the upper passage to the goal.") : TEXT("Hold RMB until the bomb bursts."),30,252,.67f,Ink);
        Text(Open ? TEXT("Leave it fully inside and still.") : TEXT("2  Step on the plate on the right."),30,285,.67f,Ink);
        Text(Open ? TEXT("Only explosions move the crate.") : TEXT("The gate stays open until R."),30,308,.67f,Muted);
        Text(TEXT("CONTROLS"),30,353,.77f,Cyan);
        Text(TEXT("WASD   Move     Mouse   Aim"),30,386,.66f,Ink);
        Text(TEXT("Hold RMB   Draw / burn fuse"),30,413,.66f,Ink);
        Text(TEXT("Release RMB   Drop at your feet"),30,440,.66f,Ink);
        Text(TEXT("LMB   Throw bomb"),30,467,.66f,Ink);
        Text(TEXT("R   Restart     Esc   Pause"),30,504,.66f,Muted);
        Text(TEXT("Unlimited bombs. One at a time."),30,532,.59f,Muted);
    }
    else
    {
        DrawRect(Panel,0,Canvas->SizeY-74*S,Canvas->SizeX,74*S);
        const float Y=Canvas->SizeY/S-66;
        Text(TEXT("WASD move | Mouse aim | Hold RMB: fuse | Release: drop | LMB: throw | R reset | Esc pause"),12,Y,.55f,Ink);
        Text(Mode->Gate && Mode->Gate->bOpen ? TEXT("Gate open: blast the crate through the upper passage into the green goal.")
            : TEXT("On the lower launch pad, face LEFT and hold RMB to jump. Then step on the right plate."),12,Y+30,.55f,Cyan);
    }
    Label(TEXT("DELIVER HERE"),FVector(665,-700,8),FLinearColor(.04f,.22f,.13f));
    Label(Mode->Gate && Mode->Gate->bOpen ? TEXT("OPEN") : TEXT("GATE"),FVector(-100,-735,140),Ink);
    Label(Mode->Gate && Mode->Gate->bOpen ? TEXT("ACTIVATED") : TEXT("STEP HERE"),FVector(300,-555,15),FLinearColor(.05f,.25f,.18f));
    Label(TEXT("FACE LEFT  <"),FVector(65,600,8),FLinearColor(.35f,.22f,.04f));
    Label(TEXT("HOLD RMB"),FVector(65,735,8),FLinearColor(.35f,.22f,.04f));
    Label(TEXT("PIT"),FVector(400,675,10),Muted);
    if (IsValid(Mode->ActiveBomb))
    {
        const float Remaining = Mode->ActiveBomb->GetRemainingFuse();
        const float Width = 235*S;
        DrawRect(Panel,16*S,Canvas->SizeY-76*S,262*S,60*S);
        DrawText(FString::Printf(TEXT("FUSE  %.1fs%s"),Remaining,Mode->ActiveBomb->IsHeld()?TEXT("  /  holding"):TEXT("")),
            Ink,30*S,Canvas->SizeY-67*S,GEngine->GetMediumFont(),.7f*S);
        DrawRect(FLinearColor(.2f,.25f,.26f),30*S,Canvas->SizeY-35*S,Width,6*S);
        DrawRect(FLinearColor(1.f,.49f,.07f),30*S,Canvas->SizeY-35*S,Width*FMath::Clamp(Remaining/Mode->FuseSeconds,0.f,1.f),6*S);
    }
    if (UGameplayStatics::IsGamePaused(this) || Mode->bCompleted)
    {
        DrawRect(FLinearColor(0,0,0,.5f),0,0,Canvas->SizeX,Canvas->SizeY);
        const float X=Canvas->SizeX*.5f-240*S, Y=Canvas->SizeY*.5f-80*S;
        DrawRect(Panel,X,Y,480*S,160*S);
        DrawText(Mode->bCompleted?TEXT("DELIVERY COMPLETE!"):TEXT("PAUSED"),Cyan,X+28*S,Y+25*S,GEngine->GetMediumFont(),1.4f*S);
        DrawText(Mode->bCompleted?FString::Printf(TEXT("%d bombs  /  %02d:%02d   -   Press R to replay"),Mode->BombsUsed,Seconds/60,Seconds%60)
            :TEXT("Esc to resume    /    R to restart"),Ink,X+28*S,Y+90*S,GEngine->GetMediumFont(),.8f*S);
    }
}
