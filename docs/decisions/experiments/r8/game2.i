%module bwapi_game2
%{
#include <BWAPI/Game.h>
%}
%varargs(const char *arg) BWAPI::Game::printf;
%ignore BWAPI::Game::printf;
%ignore BWAPI::Game::sendText;
%ignore BWAPI::Game::sendTextEx;
%ignore BWAPI::Game::drawText;
%ignore BWAPI::Game::drawTextMap;
%ignore BWAPI::Game::drawTextMouse;
%ignore BWAPI::Game::drawTextScreen;
%ignore BWAPI::Game::vPrintf;
%ignore BWAPI::Game::vSendText;
%ignore BWAPI::Game::vSendTextEx;
%ignore BWAPI::Game::vDrawText;
%include <BWAPI/Game.h>
