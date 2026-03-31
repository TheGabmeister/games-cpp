#pragma once

#include "gameState.h"

ItemType rollRandomItem(int place, int totalKarts);
int findKartAhead(const GameState &game, int kartIndex);
void useItem(GameState &game, int kartIndex);
void updateItemSystems(GameState &game, float deltaTime);
