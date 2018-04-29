#include "ESP.h"
#include "..\Utils\Utils.h"
#include "..\SDK\IVEngineClient.h"
#include "..\SDK\PlayerInfo.h"

ESP g_ESP;

void ESP::RenderBox(C_BaseEntity* pEnt)
{
    Vector vecBottom;
    Vector vecOrigin = vecBottom = pEnt->GetOrigin();

    vecBottom.z += (pEnt->GetFlags() & FL_DUCKING) ? 55.f : 74.f;

    Vector vecScreenBottom;
    if (!Utils::WorldToScreen(vecBottom, vecScreenBottom))
        return;

    Vector vecScreenOrigin;
    if (!Utils::WorldToScreen(vecOrigin, vecScreenOrigin))
        return;


    float sx = vecScreenOrigin.x;
    float sy = vecScreenOrigin.y;
    float h  = vecScreenBottom.y - vecScreenOrigin.y;
    float w  = h * 0.25f;

    /* Draw rect around the entity */
    g_Render.Rect(sx - w, sy, sx + w, sy + h, (pEnt->GetTeam() == localTeam) ? teamColor : enemyColor);

    /* Draw rect outline */
    //g_Render.Rect(sx - w - 1, sy - 1, sx + w + 1, sy + h + 1, Color::Black());
    g_Render.Rect(sx - w + 1, sy + 1, sx + w - 1, sy + h - 1, Color::Grey());
}

void ESP::RenderName(C_BaseEntity* pEnt, int iterator)
{
	Vector vecBottom; 
	Vector vecOrigin = vecBottom = pEnt->GetOrigin(); 

	PlayerInfo_t pInfo;
	g_pEngine->GetPlayerInfo(iterator, &pInfo);

	vecBottom.z += (pEnt->GetFlags() & FL_DUCKING) ? 55.f : 74.f; //diff sizes when ppl crouch (cant use 80f for height cuz it re-breaks esp)

	Vector vecScreenBottom;
	if (!Utils::WorldToScreen(vecBottom, vecScreenBottom))
		return;

	Vector vecScreenOrigin;
	if (!Utils::WorldToScreen(vecOrigin, vecScreenOrigin))
		return;

	float sx = vecScreenOrigin.x;
	float sy = vecScreenOrigin.y;
	float h = vecScreenBottom.y - vecScreenOrigin.y;

	g_Render.String(sx, sy + (h - 16) , CD3DFONT_CENTERED_X | CD3DFONT_DROPSHADOW, //pos&font
		(localTeam == pEnt->GetTeam()) ? teamColor : enemyColor, //get team & color
		g_Fonts.pFontTahoma10.get(), pInfo.szName); //font & name
}

void ESP::RenderWeaponName(C_BaseEntity* pEnt)
{
	Vector vecBottom;
	Vector vecOrigin = vecBottom = pEnt->GetOrigin();

	vecBottom.z += (pEnt->GetFlags() & FL_DUCKING) ? 55.f : 74.f;

	Vector vecScreenBottom;
	if (!Utils::WorldToScreen(vecBottom, vecScreenBottom))
		return;

	Vector vecScreenOrigin;
	if (!Utils::WorldToScreen(vecOrigin, vecScreenOrigin))
		return;

	float sx = vecScreenOrigin.x;
	float sy = vecScreenOrigin.y;
	float h = vecScreenBottom.y - vecScreenOrigin.y;

    auto weapon = pEnt->GetActiveWeapon();

    if (!weapon)
        return;

    std::string strWeaponName = weapon->GetName();
    strWeaponName.erase(0, 7);

    const auto stringToUpper = [](std::string strToConv) -> std::string
    {
        std::string tmp{};
        for (std::string::iterator p = strToConv.begin(); strToConv.end() != p; ++p)
        {
            *p = toupper(*p);
            tmp.push_back(*p);
        }

        return tmp;
    };

    g_Render.String(sx, sy, CD3DFONT_CENTERED_X | CD3DFONT_DROPSHADOW, 
		(localTeam == pEnt->GetTeam()) ? teamColor : enemyColor, 
		g_Fonts.pFontTahoma10.get(), stringToUpper(strWeaponName).c_str());
}

void ESP::Render()
{
    if (!g::pLocalEntity || !g_pEngine->IsInGame())
        return;

    localTeam = g::pLocalEntity->GetTeam();

    for (int it = 1; it <= g_pEngine->GetMaxClients(); ++it)
    {
        C_BaseEntity* pPlayerEntity = g_pEntityList->GetClientEntity(it);


        if (!pPlayerEntity
            || !pPlayerEntity->IsAlive()
            || pPlayerEntity->IsDormant()
            || pPlayerEntity == g::pLocalEntity)
            continue;


        if (g_Settings.bShowBoxes)
            this->RenderBox(pPlayerEntity);

        if (g_Settings.bShowNames)
            this->RenderName(pPlayerEntity, it);

        if (g_Settings.bShowWeapons)
            this->RenderWeaponName(pPlayerEntity);
    }
}
