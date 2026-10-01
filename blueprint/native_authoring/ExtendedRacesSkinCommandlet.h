#pragma once

#include "Commandlets/Commandlet.h"
#include "ExtendedRacesSkinCommandlet.generated.h"

/** Authors the runtime Blueprint for the Extended Races first-person skin fix. */
UCLASS()
class UExtendedRacesSkinCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UExtendedRacesSkinCommandlet();
    virtual int32 Main(const FString& Params) override;
};
