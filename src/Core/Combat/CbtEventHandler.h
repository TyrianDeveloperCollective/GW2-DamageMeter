#pragma once

#include <Game/Combat/Tracker/CombatTracker.h>
#include <Game/VdfContext.h>

struct CombatEventHandler : private GW2RE::ICombatTrackerNotificationHandler,
                            GW2RE::IVdfNotificationHandler {
  CombatEventHandler();
  ~CombatEventHandler();

private:
  void OnCombatTrackerNotifierCleanup() override;
  void OnGameViewChanged(GW2RE::UNKNOWN, GW2RE::EGameView) override;
  void OnCombatEventCreated(GW2RE::CbtEvent_t *) override;
  void OnVdfAdvanced() override;

  bool IsAttached = false;
};
