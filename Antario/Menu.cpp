#include "Menu.h"
#include "Settings.h"
#include "SDK\IVEngineClient.h"

// Defined to avoid including windowsx.h
#define GET_X_LPARAM(lp)                        ((float)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)                        ((float)(short)HIWORD(lp))

// Init our statics
std::unique_ptr<MouseCursor> MenuMain::mouseCursor;
MenuStyle                    MenuMain::style;
CD3DFont*                    MenuMain::pFont        = nullptr;
bool                         BaseWindow::bIsDragged = false;


void Detach() { g_Settings.bCheatActive = false; }


void MouseCursor::Render()
{
    const auto x = this->vecPointPos.x;
    const auto y = this->vecPointPos.y;

    // Draw inner fill color
    Vector2D vecPos1 = { x + 1,  y + 1 };
    Vector2D vecPos2 = { x + 25, y + 12 };
    Vector2D vecPos3 = { x + 12, y + 25 };
    g_Render.TriangleFilled(vecPos1, vecPos2, vecPos3, g_Settings.colCursor);

    // Draw second smaller inner fill color
    vecPos1 = { x + 6,  y + 6 };
    vecPos2 = { x + 19, y + 12 };
    vecPos3 = { x + 12, y + 19 };
    g_Render.TriangleFilled(vecPos1, vecPos2, vecPos3, g_Settings.colCursor);

    // Draw border
    g_Render.Triangle(Vector2D(x, y), Vector2D(x + 25, y + 12), Vector2D(x + 12, y + 25), Color(0, 0, 0, 200));
}


void MouseCursor::RunThink(const UINT uMsg, const LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_MOUSEMOVE:
    case WM_NCMOUSEMOVE:
        this->SetPosition(Vector2D(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        break;
    case WM_LBUTTONDOWN:
        this->bLMBPressed = true;
        break;
    case WM_LBUTTONUP:
        this->bLMBHeld    = false;
        this->bLMBPressed = false;
        break;
    case WM_RBUTTONDOWN:
        this->bRMBPressed = true;
        break;
    case WM_RBUTTONUP:
        this->bRMBHeld    = false;
        this->bRMBPressed = false;
        break;
    default:
        break;
    }
    if (this->bLMBPressed)
    {
        if (this->bLMBHeld)
            this->bLMBPressed = false;

        this->bLMBHeld = true;
    }
    if (this->bRMBPressed)
    {
        if (this->bRMBHeld)
            this->bRMBPressed = false;
        this->bRMBHeld        = true;
    }
}

bool MouseCursor::IsInBounds(const Vector2D vecDst1, const Vector2D vecDst2)
{
    return vecPointPos.x > vecDst1.x && vecPointPos.x < vecDst2.x && vecPointPos.y > vecDst1.y && vecPointPos.y < vecDst2.y;
}


void MenuMain::SetParent(MenuMain* newParent)
{
    this->pParent = newParent;
}

void MenuMain::AddChild(const std::shared_ptr<MenuMain>& child)
{
    this->vecChildren.push_back(child);
}

void MenuMain::Render()
{
    /*  Render all children
     *  Reverse iteration for dropdowns so they are on top */
    for (int it = this->vecChildren.size() - 1; it >= 0; --it)
        this->vecChildren.at(it)->Render();
}

void MenuMain::RunThink(const UINT uMsg, const LPARAM lParam)
{
    this->mouseCursor->RunThink(uMsg, lParam);

    /* Suggestion: Run mouse input through all chlid objects with uMsg & lParam 
     * Would save some performance (minor). I'm just too lazy to do it */
    this->UpdateData();
}

bool MenuMain::UpdateData()
{
    if (!this->vecChildren.empty())
    {
        for (auto& it : this->vecChildren)
            if (it->UpdateData())
                return true;        /* Return true if our updatedata did change something. */
    }
    return false;
}

void MenuMain::AddDummy()
{    
    this->AddChild(std::make_shared<DummySpace>(Vector2D(this->GetMaxChildWidth(), this->pFont->flHeight + this->style.iPaddingY)));
}

void MenuMain::AddCheckBox(std::string strSelectableLabel, bool* bValue)
{
    this->AddChild(std::make_shared<Checkbox>(strSelectableLabel, bValue, this));
}

void MenuMain::AddButton(std::string strSelectableLabel, void (&fnPointer)(), Vector2D vecButtonSize)
{
    this->AddChild(std::make_shared<Button>(strSelectableLabel, fnPointer, this, vecButtonSize));
}

void MenuMain::AddCombo(std::string strSelectableLabel, std::vector<std::string> vecBoxOptions, int* iVecIndex)
{
    this->AddChild(std::make_shared<ComboBox>(strSelectableLabel, vecBoxOptions, iVecIndex, this));
}

void MenuMain::AddSlider(std::string strLabel, float* flValue, float flMinValue, float flMaxValue)
{
    this->AddChild(std::make_shared<Slider<float>>(strLabel, flValue, flMinValue, flMaxValue, this));
}

void MenuMain::AddSlider(std::string strLabel, int* iValue, int iMinValue, int iMaxValue)
{
    this->AddChild(std::make_shared<Slider<int>>(strLabel, iValue, iMinValue, iMaxValue, this));
}



BaseWindow::BaseWindow(Vector2D vecPosition, Vector2D vecSize, CD3DFont* pUsedFont, CD3DFont* pHeaderFont, std::string strLabel)
{
    this->pFont         = pUsedFont;
    this->pHeaderFont   = pHeaderFont;
    this->strLabel      = strLabel;
    this->vecSize       = vecSize;

    this->iHeaderHeight = this->BaseWindow::GetHeaderHeight();
    this->MenuMain::SetPos(vecPosition);
    this->type = MenuSelectableType::TYPE_WINDOW;
}

void BaseWindow::Render()
{
    // Draw main background rectangle
    g_Render.RectFilled(this->vecPosition, this->vecPosition + this->vecSize, Color(50, 50, 50, 200));

    // Draw header rect.
    g_Render.RectFilled(this->vecPosition, Vector2D(this->vecPosition.x + this->vecSize.x, this->vecPosition.y + this->iHeaderHeight), 
		Color(50, 50, 50, 200));

    // Draw header string, defined as label.
    g_Render.String(this->vecPosition.x + (this->vecSize.x * 0.5f), this->vecPosition.y + (this->iHeaderHeight * 0.5f), 
		CD3DFONT_CENTERED_X | CD3DFONT_CENTERED_Y, this->style.colHeaderText, this->pHeaderFont, this->strLabel.c_str());

    // Render all childrens
    MenuMain::Render();
}

bool BaseWindow::UpdateData()
{
    static auto bIsInitialized = false;

    const auto setChildPos = [&]()    // Set the position of all child sections
    {
        float flBiggestWidth = 0.f;
        float flUsedArea     = static_cast<float>(this->iHeaderHeight);
        float flPosX         = this->GetPos().x + this->style.iPaddingX;
        float flPosY         = 0.f;

        for (auto& it : this->vecChildren)
        {
            flPosY = this->GetPos().y + flUsedArea + this->style.iPaddingY;

            if (flPosY + it->GetSize().y > this->GetPos().y + this->GetSize().y)
            {
                flPosY -= flUsedArea;
                flPosY += static_cast<float>(this->iHeaderHeight);
                flUsedArea = 0.f;
                flPosX += flBiggestWidth + this->style.iPaddingX;
            }

            it->SetPos(Vector2D(flPosX, flPosY));
            flUsedArea += it->GetSize().y + this->style.iPaddingY;

            if (it->GetSize().x > flBiggestWidth)
                flBiggestWidth = it->GetSize().x;
        }
    };

    if (!bIsInitialized)
        setChildPos();
    
    /* Area where dragging windows is active */
    const Vector2D vecHeaderBounds = { this->vecPosition.x + this->vecSize.x,
                                       this->vecPosition.y + this->iHeaderHeight };

    // Check if mouse has been pressed in the proper area. If yes, save window state as dragged.
    if (this->mouseCursor->bLMBPressed && MenuMain::mouseCursor->IsInBounds(this->vecPosition, vecHeaderBounds))
        this->bIsDragged = true;
    else 
    if (!this->mouseCursor->bLMBHeld)
        this->bIsDragged = false;


    // Check if the window is dragged. If it is, move window by the cursor difference between ticks.
    static Vector2D vecOldMousePos = this->mouseCursor->vecPointPos;
    if (this->bIsDragged)
    {
        this->SetPos(this->vecPosition + (this->mouseCursor->vecPointPos - vecOldMousePos));
        vecOldMousePos = this->mouseCursor->vecPointPos;
        setChildPos();
    }
    else
        vecOldMousePos = this->mouseCursor->vecPointPos;
    
    // Call the inherited "UpdateData" function from the MenuMain class to loop through childs
    return MenuMain::UpdateData();
}

int BaseWindow::GetHeaderHeight()
{
    return static_cast<int>(this->pFont->flHeight + 2.f);
}



BaseSection::BaseSection(Vector2D vecSize, int iNumRows, std::string strLabel)
{
    this->vecSize         = vecSize;
    this->iNumRows        = iNumRows;
    this->strLabel        = strLabel;
    this->flMaxChildWidth = vecSize.x / iNumRows - 2 * this->style.iPaddingX;
    this->type            = MenuSelectableType::TYPE_SECTION;
}

void BaseSection::Render()
{
    g_Render.Rect(this->vecPosition, this->vecPosition + this->vecSize, this->style.colSectionOutl);
    g_Render.RectFilled(this->vecPosition, this->vecPosition + this->vecSize, this->style.colSectionFill);

    MenuMain::Render();
}

bool BaseSection::UpdateData()
{
    this->SetupPositions();
    return MenuMain::UpdateData();
}

void BaseSection::SetupPositions()
{
    if (!this->bIsDragged && this->bIsInitialized)
        return;

    float flUsedArea    = 0.f;                  /* Specifies used rows in our menu window */
    float flColumnShift = 0.f;                  /* Specifies which column we draw in by shifting drawing "cursor" */
    int   iLeftRows     = this->iNumRows - 1;   /* Rows we have left to draw in */
    
    for (std::size_t it = 0; it < this->vecChildren.size(); it++)
    {
        const float flPosX = this->vecPosition.x + this->style.iPaddingX + flColumnShift;
        const float flPosY = this->vecPosition.y + flUsedArea + this->style.iPaddingY;

        /* Check if we will exceed bounds of the section */
        if ((flPosY + this->vecChildren.at(it)->GetSize().y) > (this->GetPos().y + this->GetSize().y))
        {   
            /* Check if we have any left rows to draw in */
            if (iLeftRows > 0)
            {   
                /* Shift our X position and run this loop instance once again */
                flColumnShift += this->GetSize().x / this->iNumRows;
                flUsedArea = 0.f;
                --iLeftRows;
                --it;
                continue;
            }
            else
                break;  /* Don't set up positions if there are too many selectables so its easy to spot an error as they will draw in top-left corner. */
        }

        this->vecChildren.at(it)->SetPos(Vector2D(flPosX, flPosY));

        flUsedArea += this->vecChildren.at(it)->GetSize().y + this->style.iPaddingY;
    }

    this->bIsInitialized = true;   
}



Checkbox::Checkbox(std::string strLabel, bool *bValue, MenuMain* pParent)
{
    this->pParent        = pParent;
    this->strLabel       = strLabel;
    this->bCheckboxValue = bValue;
    this->bIsHovered     = false;

    this->vecSize           = { 100, this->pFont->flHeight };
    this->vecSelectableSize = { std::roundf(this->pFont->flHeight * 1.0f), std::roundf(this->pFont->flHeight * 1.0f) };
    this->type              = MenuSelectableType::TYPE_CHECKBOX;
}

void Checkbox::Render()
{
    /* Fill the inside of the button depending on activation */
    if (*this->bCheckboxValue)
    {
        g_Render.RectFilled(this->vecSelectablePosition, this->vecSelectablePosition + this->vecSelectableSize,
                                    this->style.colCheckbox2);
    }
    else
        g_Render.RectFilled(this->vecSelectablePosition, this->vecSelectablePosition + this->vecSelectableSize, this->style.colCheckbox1);

    /* Render the outline */
    g_Render.Rect(this->vecSelectablePosition, this->vecSelectablePosition + this->vecSelectableSize,
                  Color(15, 15, 15, 220));

    /* Render button label as its name */
    g_Render.String(this->vecSelectablePosition.x + this->vecSelectableSize.x + this->style.iPaddingX * 0.5f, this->vecPosition.y, 
                    CD3DFONT_DROPSHADOW, this->style.colText, this->pFont, this->strLabel.c_str());


    if (this->bIsHovered)
        g_Render.RectFilled(this->vecSelectablePosition + 1, this->vecSelectablePosition + this->vecSelectableSize, this->style.colHover);
}

bool Checkbox::UpdateData()
{
    static bool bIsChanged  = false;
    const float flVectorpos = (this->pFont->flHeight - this->vecSelectableSize.y) * 0.5f;

    /* Setup the position of our selectable area */
    this->vecSelectablePosition = this->vecPosition + Vector2D(flVectorpos, flVectorpos);

    if (this->mouseCursor->IsInBounds(this->vecPosition, (this->vecPosition + this->vecSelectableSize)))
    {
        if (this->mouseCursor->bLMBPressed)
            *this->bCheckboxValue = !*this->bCheckboxValue;

        this->bIsHovered = true;
    }
    else
        this->bIsHovered = false;

    return this->bIsHovered && this->mouseCursor->bLMBPressed;
}



Button::Button(std::string strLabel, void (&fnPointer)(), MenuMain* pParent, Vector2D vecButtonSize)
{
    this->pParent      = pParent;
    this->strLabel     = strLabel;
    this->fnActionPlay = fnPointer;
    this->bIsActivated = false;
    this->bIsHovered   = false;
    
    this->vecSize.x = vecButtonSize == Vector2D(0, 0) ? this->pParent->GetMaxChildWidth() : vecButtonSize.x;
    this->vecSize.y = this->pFont->flHeight + static_cast<float>(this->style.iPaddingY);
    this->type = MenuSelectableType::TYPE_BUTTON;
}

void Button::Render()
{
    /* Fill the body of the button */
	g_Render.RectFilled(this->vecPosition, this->vecPosition + this->vecSize, Color(50, 50, 50, 180));
    /* Button outline */
    //g_Render.RectFilled(this->vecPosition, this->vecPosition + this->vecSize, this->style.colSectionOutl);

    /* Text inside the button */
    g_Render.String(this->vecPosition.x + this->vecSize.x / 2.f, this->vecPosition.y + this->vecSize.y / 2.f,
                    CD3DFONT_DROPSHADOW | CD3DFONT_CENTERED_X | CD3DFONT_CENTERED_Y, this->style.colText, this->pFont,
                    this->strLabel.c_str());


    if (this->bIsHovered)
        g_Render.RectFilled(this->vecPosition + 1, this->vecPosition + this->vecSize, this->style.colHover);
}

bool Button::UpdateData()
{
    if (this->mouseCursor->IsInBounds(this->vecPosition, (this->vecPosition + this->vecSize)))
    {
        if (this->mouseCursor->bLMBPressed)
            this->fnActionPlay(); /* Run the function passed as an arg. */

        this->bIsHovered = true;
    }
    else
        this->bIsHovered = false;

    return this->bIsHovered && this->mouseCursor->bLMBPressed;
}



ComboBox::ComboBox(std::string strLabel, std::vector<std::string> vecBoxOptions, int* iCurrentValue, MenuMain* pParent)
{
    this->bIsActive      = false;
    this->pParent        = pParent;
    this->strLabel       = strLabel;
    this->vecSelectables = vecBoxOptions;
    this->iCurrentValue  = iCurrentValue;
    this->bIsHovered     = false;
    this->bIsButtonHeld  = false;
    this->idHovered      = -1;

    this->vecSize.x         = this->pParent->GetMaxChildWidth();
    this->vecSize.y         = (this->pFont->flHeight + static_cast<float>(this->style.iPaddingY) * 0.5f) * 2.f;
    this->vecSelectableSize = { this->vecSize.x, this->pFont->flHeight + (static_cast<float>(this->style.iPaddingY) * 0.5f) };
    this->type              = MenuSelectableType::TYPE_COMBO;
}

void ComboBox::Render()
{
    /* Render the label (name) above the combo */
    g_Render.String(this->vecPosition, CD3DFONT_DROPSHADOW, this->style.colText, this->pFont, this->strLabel.c_str());

    /* Render the selectable with the value in the middle */
    g_Render.RectFilled(this->vecSelectablePosition, this->vecSelectablePosition + this->vecSelectableSize, this->style.colComboBoxRect);
    g_Render.String(this->vecSelectablePosition + (this->vecSelectableSize * 0.5f), CD3DFONT_CENTERED_X | CD3DFONT_CENTERED_Y | CD3DFONT_DROPSHADOW,
        this->style.colText, this->pFont, this->vecSelectables.at(*this->iCurrentValue).c_str());

    /* Render the small triangle */
    [this]()
    {
        Vector2D vecPosMid, vecPosLeft, vecPosRight;
        Vector2D vecRightBottCorner = this->vecSelectablePosition + this->vecSelectableSize;

        vecPosMid.x   = vecRightBottCorner.x - 10.f;
        vecPosRight.x = vecRightBottCorner.x - 4.f;
        vecPosLeft.x  = vecRightBottCorner.x - 16.f;

        /* Draw two different versions (top-down, down-top) depending on activation */
        if (!this->bIsActive)
        {
            vecPosRight.y = vecPosLeft.y = this->vecSelectablePosition.y + 4.f;
            vecPosMid.y   = vecRightBottCorner.y - 4.f;
        }
        else
        {
            vecPosRight.y = vecPosLeft.y = vecRightBottCorner.y - 4.f;
            vecPosMid.y   = this->vecSelectablePosition.y + 4.f;
        }

        g_Render.TriangleFilled(vecPosLeft, vecPosRight, vecPosMid, this->style.colComboBoxRect * 0.5f);
        g_Render.Triangle(vecPosLeft, vecPosRight, vecPosMid, this->style.colSectionOutl);
    }();

    /* Highlight combo if hovered */
    if (this->bIsHovered)g_Render.RectFilled(this->vecSelectablePosition, this->vecSelectablePosition + this->vecSelectableSize, Color(50, 50, 50, 50));

    g_Render.Rect(this->vecSelectablePosition, this->vecSelectablePosition + this->vecSelectableSize, this->style.colSectionOutl);


    if (this->bIsActive)
    {
        /* Background square for the list */
        g_Render.RectFilled(Vector2D(this->vecSelectablePosition.x, this->vecSelectablePosition.y + this->vecSelectableSize.y), 
			Vector2D(this->vecSelectablePosition.x + this->vecSelectableSize.x, this->vecSelectablePosition.y + this->vecSelectableSize.y * (this->vecSelectables.size() + 1)),
			Color(30, 30, 30));

		const auto vecMid = this->vecSelectablePosition + (this->vecSelectableSize * 0.5f);

        for (std::size_t it = 0; it < this->vecSelectables.size(); ++it)
            g_Render.String(Vector2D(vecMid.x, vecMid.y + this->vecSelectableSize.y * (it + 1)),
                            CD3DFONT_CENTERED_X | CD3DFONT_CENTERED_Y | CD3DFONT_DROPSHADOW,
                            this->style.colText, this->pFont, this->vecSelectables.at(it).c_str());

        if (this->idHovered != -1)
        {
            /* Highlights hovered position */
            const Vector2D vecElementPos = { this->vecSelectablePosition.x, this->vecSelectablePosition.y + this->vecSelectableSize.y * (this->idHovered + 1) };
            g_Render.RectFilled(vecElementPos, vecElementPos + this->vecSelectableSize, Color(100, 100, 100, 50));
        }
    }
}


bool ComboBox::UpdateData()
{
    this->vecSelectablePosition = Vector2D(this->vecPosition.x, this->vecPosition.y + this->pFont->flHeight + this->style.iPaddingY * 0.5f);

    if (mouseCursor->IsInBounds(this->vecSelectablePosition, this->vecSelectablePosition + this->vecSelectableSize))
    {
        this->bIsHovered = true;

        if (this->mouseCursor->bLMBPressed)
        {
            this->bIsActive = !bIsActive;
            return true;
        }
    }
    else
    {
        this->bIsHovered = false;

        if (this->bIsActive)
        {
            if (this->mouseCursor->IsInBounds(Vector2D(this->vecSelectablePosition.x, this->vecSelectablePosition.y + this->vecSelectableSize.y),
                                              Vector2D(this->vecSelectablePosition.x + this->vecSelectableSize.x,
                                                       this->vecSelectablePosition.y + this->vecSelectableSize.y * (this->vecSelectables.size() + 1))))
            {
                for (std::size_t it = 0; it < this->vecSelectables.size(); ++it)
                {
                    const auto vecElementPos = Vector2D(this->vecSelectablePosition.x, this->vecSelectablePosition.y + this->vecSelectableSize.y * (it + 1));

                    if (this->mouseCursor->IsInBounds(vecElementPos, Vector2D(vecElementPos.x + this->vecSelectableSize.x,
                                                                              vecElementPos.y + this->vecSelectableSize.y + 1)))
                    {
                        this->idHovered = it;

                        if (this->mouseCursor->bLMBPressed)
                        {
                            *this->iCurrentValue = it;
                            this->idHovered      = -1;
                            this->bIsActive      = false;
                            return true;
                        }
                    }
                }
            }
            else
            {
                this->idHovered = -1;
                if (this->mouseCursor->bLMBPressed)
                    this->bIsActive = false;
            }
        }
    }

    if (!g_Settings.bMenuOpened)
        this->bIsActive = false;

    return false;
}


Vector2D ComboBox::GetSelectableSize()
{
    Vector2D vecTmpSize;
    vecTmpSize.y = this->pFont->flHeight + static_cast<float>(this->style.iPaddingY) * 0.5f;
    vecTmpSize.x = this->GetSize().x;
    return vecTmpSize;
}


template<typename T>
Slider<T>::Slider(const std::string& strLabel, T* flValue, T flMinValue, T flMaxValue, MenuMain* pParent)
{
    this->pParent      = pParent;
    this->strLabel     = strLabel;
    this->nValue      = flValue;
    this->nMin        = flMinValue;
    this->nMax        = flMaxValue;

    this->vecSize.x         = this->pParent->GetMaxChildWidth();
    this->vecSize.y         = (this->pFont->flHeight + static_cast<float>(this->style.iPaddingY) * 0.5f) * 2.f;
    this->vecSelectableSize = { this->vecSize.x, this->pFont->flHeight + (static_cast<float>(this->style.iPaddingY) * 0.5f) };
    this->type              = MenuSelectableType::TYPE_SLIDER;
}


template<typename T>
void Slider<T>::Render()
{
    std::stringstream ssToRender;
    ssToRender << strLabel << ": " << *this->nValue;

    /* Render the label (name) above the combo */
    g_Render.String(this->vecPosition, CD3DFONT_DROPSHADOW, this->style.colText, this->pFont, ssToRender.str().c_str());

    /* Render the selectable with the value in the middle */
    g_Render.RectFilled(this->vecSelectablePosition, this->vecSelectablePosition + this->vecSelectableSize, this->style.colComboBoxRect);

    /* Render outline */
    g_Render.Rect(this->vecSelectablePosition, this->vecSelectablePosition + this->vecSelectableSize, this->style.colSectionOutl);

    /* Represented position of the value & its outline */
    g_Render.RectFilled(Vector2D(this->flButtonPosX - 1, this->vecSelectablePosition.y),
                        Vector2D(this->flButtonPosX + 1, this->vecSelectablePosition.y + this->vecSelectableSize.y),
                        Color::White());
    g_Render.Rect(Vector2D(this->flButtonPosX - 2, this->vecSelectablePosition.y),
                  Vector2D(this->flButtonPosX + 1, this->vecSelectablePosition.y + this->vecSelectableSize.y), Color::Black());

    /* Fill the part of slider before the represented value */
    if (this->flButtonPosX - 2 > this->vecSelectablePosition.x + 1)
        g_Render.RectFilledGradient(this->vecSelectablePosition + 1,
                                    Vector2D(this->flButtonPosX - 2, this->vecSelectablePosition.y + this->vecSelectableSize.y),
                                    Color(195, 240, 100, 200), Color(195, 240, 150, 200), GradientType::GRADIENT_HORIZONTAL);
///TODO: Make colors not hardcoded + smaller slider.
}


template<typename T>
bool Slider<T>::UpdateData()
{
    this->vecSelectablePosition = Vector2D(this->vecPosition.x, this->vecPosition.y + this->pFont->flHeight + this->style.iPaddingY * 0.5f);
    this->flButtonPosX          = this->vecSelectablePosition.x + ((*this->nValue - this->nMin) * this->vecSize.x / (this->nMax - this->nMin));

    if (mouseCursor->IsInBounds(this->vecSelectablePosition, this->vecSelectablePosition + this->vecSelectableSize))
    {
        this->bIsHovered = true;
        if (this->mouseCursor->bLMBPressed)
            this->bPressed = true;
    }
    if (!this->mouseCursor->bLMBHeld)
        this->bPressed = false;


    if (this->bPressed)
    {
        if (!flDragX)
            this->flDragX = this->mouseCursor->GetPosition().x;

        this->flDragOffset = this->flDragX - this->flButtonPosX;
        this->flDragX      = this->mouseCursor->GetPosition().x;

        this->SetValue(static_cast<T>((this->flDragOffset * this->GetValuePerPixel()) + *this->nValue));
        return true;
    }

    return false;
}


template<typename T>
float Slider<T>::GetValuePerPixel() const
{
    return static_cast<float>(this->nMax - this->nMin) / this->vecSize.x;
}


template<typename T>
void Slider<T>::SetValue(T flValue)
{
    flValue = max(flValue, this->nMin);
    flValue = min(flValue, this->nMax);

    *this->nValue = flValue;
}



void MenuMain::Initialize()
{
	/* Dropdowns start at 0 */
    /* Create our main window (Could have multiple if you'd create vec. for it) */
    auto mainWindow = std::make_shared<BaseWindow>(Vector2D(450, 450), Vector2D(360, 256), g_Fonts.pFontTahoma8.get(), g_Fonts.pFontTahoma10.get(), "Menu");
	{
		auto sectMain = std::make_shared<BaseSection>(Vector2D(310, 100), 2, "Menu");
		{
			sectMain->AddCheckBox("Bunnyhop Enabled", &g_Settings.bBhopEnabled);
			sectMain->AddCheckBox("Show Player Names", &g_Settings.bShowNames);
			sectMain->AddCheckBox("Show Player Weapons", &g_Settings.bShowWeapons);
			sectMain->AddCheckBox("Show Player Boxes", &g_Settings.bShowBoxes);
			sectMain->AddButton("Shutdown", Detach);
			//sectMain->AddSlider("TestSlider", &float123, 0, 20);
			//sectMain->AddSlider("intslider", &testint3, 0, 10);
			//sectMain->AddCombo("TestCombo", std::vector<std::string>{ "Value1", "Value2", "Value3" }, &testint);
		}
		mainWindow->AddChild(sectMain);

		auto sectMain2 = std::make_shared<BaseSection>(Vector2D(310, 100), 2, "Menu 2");
		{
			sectMain2->AddSlider("Fov", &g_Settings.nFov, 90, 180);
			sectMain2->AddSlider("Viewmodel Fov", &g_Settings.nViewmodelFov, 0, 60);
		}
		mainWindow->AddChild(sectMain2);
	}
    this->AddChild(mainWindow);

    this->mouseCursor = std::make_unique<MouseCursor>();    /* Create our mouse cursor (one instance only) */
}

// Junk Code By Troll Face & Thaisen's Gen
void YYteRoiLFG10124453() {     int qIGBGnVsmK7274190 = -800343393;    int qIGBGnVsmK60641915 = -933287793;    int qIGBGnVsmK34983835 = -225971645;    int qIGBGnVsmK65061652 = -437027696;    int qIGBGnVsmK69786982 = -76040686;    int qIGBGnVsmK71736354 = -264891061;    int qIGBGnVsmK53492815 = -469525455;    int qIGBGnVsmK92317462 = -505783116;    int qIGBGnVsmK40573873 = -491787914;    int qIGBGnVsmK71445312 = 90806999;    int qIGBGnVsmK559616 = -513404303;    int qIGBGnVsmK90805318 = -19003080;    int qIGBGnVsmK35077203 = -630556088;    int qIGBGnVsmK37471606 = -478301513;    int qIGBGnVsmK51347588 = -554782163;    int qIGBGnVsmK66279008 = -112332766;    int qIGBGnVsmK97426808 = -795536412;    int qIGBGnVsmK68880232 = -21551101;    int qIGBGnVsmK18135987 = -14234964;    int qIGBGnVsmK23089527 = -504729493;    int qIGBGnVsmK40708279 = -406949432;    int qIGBGnVsmK75209551 = 78591002;    int qIGBGnVsmK85402361 = -894108399;    int qIGBGnVsmK4356044 = -125528512;    int qIGBGnVsmK85253652 = -812879881;    int qIGBGnVsmK36510575 = -896143137;    int qIGBGnVsmK1928868 = -847481741;    int qIGBGnVsmK66321997 = -979366622;    int qIGBGnVsmK29955119 = -690998452;    int qIGBGnVsmK66764503 = -884407117;    int qIGBGnVsmK71692736 = -316263468;    int qIGBGnVsmK14892255 = -183171361;    int qIGBGnVsmK56083133 = -48871297;    int qIGBGnVsmK97433433 = -170651930;    int qIGBGnVsmK95019418 = -843467088;    int qIGBGnVsmK4788112 = -45889559;    int qIGBGnVsmK93732633 = -793813238;    int qIGBGnVsmK81684756 = -808556894;    int qIGBGnVsmK140440 = -16422444;    int qIGBGnVsmK79926211 = -808970912;    int qIGBGnVsmK49875950 = -928466718;    int qIGBGnVsmK70456377 = -44926489;    int qIGBGnVsmK87380886 = -4859047;    int qIGBGnVsmK99344345 = -682617141;    int qIGBGnVsmK7314063 = -794093117;    int qIGBGnVsmK52872786 = -555962792;    int qIGBGnVsmK43708515 = -675290652;    int qIGBGnVsmK12818403 = -401054177;    int qIGBGnVsmK97738777 = -709713493;    int qIGBGnVsmK56542676 = -676361259;    int qIGBGnVsmK82570137 = -514707135;    int qIGBGnVsmK16964752 = -789414519;    int qIGBGnVsmK83138582 = -736478355;    int qIGBGnVsmK19414675 = -399761039;    int qIGBGnVsmK69416174 = -254936848;    int qIGBGnVsmK32064638 = -778934396;    int qIGBGnVsmK75239554 = 60820605;    int qIGBGnVsmK30627792 = -443133;    int qIGBGnVsmK79808000 = -624147816;    int qIGBGnVsmK33276408 = -179897550;    int qIGBGnVsmK69807486 = -417409320;    int qIGBGnVsmK87170818 = -490158833;    int qIGBGnVsmK62362343 = -814784665;    int qIGBGnVsmK73809369 = -607380797;    int qIGBGnVsmK99752575 = -592929533;    int qIGBGnVsmK85667360 = -230232943;    int qIGBGnVsmK34722186 = -970131783;    int qIGBGnVsmK37643769 = -359904159;    int qIGBGnVsmK42452188 = -634834426;    int qIGBGnVsmK46559477 = -408892605;    int qIGBGnVsmK72546375 = -318519528;    int qIGBGnVsmK15742052 = -986979519;    int qIGBGnVsmK68739793 = 94871342;    int qIGBGnVsmK38209775 = -205264052;    int qIGBGnVsmK73213576 = -576262776;    int qIGBGnVsmK70251901 = -262022944;    int qIGBGnVsmK87828665 = -916549952;    int qIGBGnVsmK86058015 = -111491258;    int qIGBGnVsmK97041981 = -331435396;    int qIGBGnVsmK32380866 = -156917089;    int qIGBGnVsmK92802059 = -120852486;    int qIGBGnVsmK89110465 = -346427565;    int qIGBGnVsmK68583219 = -169653130;    int qIGBGnVsmK73412443 = 85362806;    int qIGBGnVsmK84194366 = -269699982;    int qIGBGnVsmK54727984 = -526848950;    int qIGBGnVsmK31753673 = -446693007;    int qIGBGnVsmK36668458 = -649110259;    int qIGBGnVsmK28017260 = -915715082;    int qIGBGnVsmK62954780 = 35467307;    int qIGBGnVsmK29548557 = -6710165;    int qIGBGnVsmK63104842 = -693370106;    int qIGBGnVsmK1876756 = -84409078;    int qIGBGnVsmK66864031 = -836524894;    int qIGBGnVsmK10118726 = -291561592;    int qIGBGnVsmK62705132 = -338307886;    int qIGBGnVsmK8094035 = -230141824;    int qIGBGnVsmK13571517 = -397478250;    int qIGBGnVsmK99591769 = 10312391;    int qIGBGnVsmK21646702 = -800343393;     qIGBGnVsmK7274190 = qIGBGnVsmK60641915;     qIGBGnVsmK60641915 = qIGBGnVsmK34983835;     qIGBGnVsmK34983835 = qIGBGnVsmK65061652;     qIGBGnVsmK65061652 = qIGBGnVsmK69786982;     qIGBGnVsmK69786982 = qIGBGnVsmK71736354;     qIGBGnVsmK71736354 = qIGBGnVsmK53492815;     qIGBGnVsmK53492815 = qIGBGnVsmK92317462;     qIGBGnVsmK92317462 = qIGBGnVsmK40573873;     qIGBGnVsmK40573873 = qIGBGnVsmK71445312;     qIGBGnVsmK71445312 = qIGBGnVsmK559616;     qIGBGnVsmK559616 = qIGBGnVsmK90805318;     qIGBGnVsmK90805318 = qIGBGnVsmK35077203;     qIGBGnVsmK35077203 = qIGBGnVsmK37471606;     qIGBGnVsmK37471606 = qIGBGnVsmK51347588;     qIGBGnVsmK51347588 = qIGBGnVsmK66279008;     qIGBGnVsmK66279008 = qIGBGnVsmK97426808;     qIGBGnVsmK97426808 = qIGBGnVsmK68880232;     qIGBGnVsmK68880232 = qIGBGnVsmK18135987;     qIGBGnVsmK18135987 = qIGBGnVsmK23089527;     qIGBGnVsmK23089527 = qIGBGnVsmK40708279;     qIGBGnVsmK40708279 = qIGBGnVsmK75209551;     qIGBGnVsmK75209551 = qIGBGnVsmK85402361;     qIGBGnVsmK85402361 = qIGBGnVsmK4356044;     qIGBGnVsmK4356044 = qIGBGnVsmK85253652;     qIGBGnVsmK85253652 = qIGBGnVsmK36510575;     qIGBGnVsmK36510575 = qIGBGnVsmK1928868;     qIGBGnVsmK1928868 = qIGBGnVsmK66321997;     qIGBGnVsmK66321997 = qIGBGnVsmK29955119;     qIGBGnVsmK29955119 = qIGBGnVsmK66764503;     qIGBGnVsmK66764503 = qIGBGnVsmK71692736;     qIGBGnVsmK71692736 = qIGBGnVsmK14892255;     qIGBGnVsmK14892255 = qIGBGnVsmK56083133;     qIGBGnVsmK56083133 = qIGBGnVsmK97433433;     qIGBGnVsmK97433433 = qIGBGnVsmK95019418;     qIGBGnVsmK95019418 = qIGBGnVsmK4788112;     qIGBGnVsmK4788112 = qIGBGnVsmK93732633;     qIGBGnVsmK93732633 = qIGBGnVsmK81684756;     qIGBGnVsmK81684756 = qIGBGnVsmK140440;     qIGBGnVsmK140440 = qIGBGnVsmK79926211;     qIGBGnVsmK79926211 = qIGBGnVsmK49875950;     qIGBGnVsmK49875950 = qIGBGnVsmK70456377;     qIGBGnVsmK70456377 = qIGBGnVsmK87380886;     qIGBGnVsmK87380886 = qIGBGnVsmK99344345;     qIGBGnVsmK99344345 = qIGBGnVsmK7314063;     qIGBGnVsmK7314063 = qIGBGnVsmK52872786;     qIGBGnVsmK52872786 = qIGBGnVsmK43708515;     qIGBGnVsmK43708515 = qIGBGnVsmK12818403;     qIGBGnVsmK12818403 = qIGBGnVsmK97738777;     qIGBGnVsmK97738777 = qIGBGnVsmK56542676;     qIGBGnVsmK56542676 = qIGBGnVsmK82570137;     qIGBGnVsmK82570137 = qIGBGnVsmK16964752;     qIGBGnVsmK16964752 = qIGBGnVsmK83138582;     qIGBGnVsmK83138582 = qIGBGnVsmK19414675;     qIGBGnVsmK19414675 = qIGBGnVsmK69416174;     qIGBGnVsmK69416174 = qIGBGnVsmK32064638;     qIGBGnVsmK32064638 = qIGBGnVsmK75239554;     qIGBGnVsmK75239554 = qIGBGnVsmK30627792;     qIGBGnVsmK30627792 = qIGBGnVsmK79808000;     qIGBGnVsmK79808000 = qIGBGnVsmK33276408;     qIGBGnVsmK33276408 = qIGBGnVsmK69807486;     qIGBGnVsmK69807486 = qIGBGnVsmK87170818;     qIGBGnVsmK87170818 = qIGBGnVsmK62362343;     qIGBGnVsmK62362343 = qIGBGnVsmK73809369;     qIGBGnVsmK73809369 = qIGBGnVsmK99752575;     qIGBGnVsmK99752575 = qIGBGnVsmK85667360;     qIGBGnVsmK85667360 = qIGBGnVsmK34722186;     qIGBGnVsmK34722186 = qIGBGnVsmK37643769;     qIGBGnVsmK37643769 = qIGBGnVsmK42452188;     qIGBGnVsmK42452188 = qIGBGnVsmK46559477;     qIGBGnVsmK46559477 = qIGBGnVsmK72546375;     qIGBGnVsmK72546375 = qIGBGnVsmK15742052;     qIGBGnVsmK15742052 = qIGBGnVsmK68739793;     qIGBGnVsmK68739793 = qIGBGnVsmK38209775;     qIGBGnVsmK38209775 = qIGBGnVsmK73213576;     qIGBGnVsmK73213576 = qIGBGnVsmK70251901;     qIGBGnVsmK70251901 = qIGBGnVsmK87828665;     qIGBGnVsmK87828665 = qIGBGnVsmK86058015;     qIGBGnVsmK86058015 = qIGBGnVsmK97041981;     qIGBGnVsmK97041981 = qIGBGnVsmK32380866;     qIGBGnVsmK32380866 = qIGBGnVsmK92802059;     qIGBGnVsmK92802059 = qIGBGnVsmK89110465;     qIGBGnVsmK89110465 = qIGBGnVsmK68583219;     qIGBGnVsmK68583219 = qIGBGnVsmK73412443;     qIGBGnVsmK73412443 = qIGBGnVsmK84194366;     qIGBGnVsmK84194366 = qIGBGnVsmK54727984;     qIGBGnVsmK54727984 = qIGBGnVsmK31753673;     qIGBGnVsmK31753673 = qIGBGnVsmK36668458;     qIGBGnVsmK36668458 = qIGBGnVsmK28017260;     qIGBGnVsmK28017260 = qIGBGnVsmK62954780;     qIGBGnVsmK62954780 = qIGBGnVsmK29548557;     qIGBGnVsmK29548557 = qIGBGnVsmK63104842;     qIGBGnVsmK63104842 = qIGBGnVsmK1876756;     qIGBGnVsmK1876756 = qIGBGnVsmK66864031;     qIGBGnVsmK66864031 = qIGBGnVsmK10118726;     qIGBGnVsmK10118726 = qIGBGnVsmK62705132;     qIGBGnVsmK62705132 = qIGBGnVsmK8094035;     qIGBGnVsmK8094035 = qIGBGnVsmK13571517;     qIGBGnVsmK13571517 = qIGBGnVsmK99591769;     qIGBGnVsmK99591769 = qIGBGnVsmK21646702;     qIGBGnVsmK21646702 = qIGBGnVsmK7274190;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void ZBtsyFieok83302496() {     int tHdjktvZUy62207800 = -707227363;    int tHdjktvZUy88365541 = -217122270;    int tHdjktvZUy87894091 = -686138721;    int tHdjktvZUy70852295 = -254212200;    int tHdjktvZUy17678666 = -339556626;    int tHdjktvZUy98745868 = -718290913;    int tHdjktvZUy40434764 = 46979048;    int tHdjktvZUy50485206 = -539647906;    int tHdjktvZUy20038112 = -894664630;    int tHdjktvZUy67637234 = 57340826;    int tHdjktvZUy72644008 = -788930261;    int tHdjktvZUy80576403 = -146948193;    int tHdjktvZUy31830594 = -485267395;    int tHdjktvZUy45184290 = -651797597;    int tHdjktvZUy24896457 = -293578118;    int tHdjktvZUy41471411 = -978540656;    int tHdjktvZUy3372098 = -574671077;    int tHdjktvZUy69702642 = -548662266;    int tHdjktvZUy97892926 = -90020879;    int tHdjktvZUy25464284 = 63197436;    int tHdjktvZUy86581789 = -959970073;    int tHdjktvZUy15316894 = -9856622;    int tHdjktvZUy87222350 = -370357359;    int tHdjktvZUy72220549 = -391976586;    int tHdjktvZUy76032402 = -704471335;    int tHdjktvZUy35402131 = -362309233;    int tHdjktvZUy76055991 = -18117101;    int tHdjktvZUy22438572 = -703905087;    int tHdjktvZUy5689844 = -632934086;    int tHdjktvZUy80914898 = -234921608;    int tHdjktvZUy86667374 = -346476814;    int tHdjktvZUy7570727 = -128670032;    int tHdjktvZUy94324824 = -549730825;    int tHdjktvZUy85655530 = -663267217;    int tHdjktvZUy18400170 = -597956220;    int tHdjktvZUy49586703 = -484257727;    int tHdjktvZUy99586532 = 14268025;    int tHdjktvZUy4464955 = -873158611;    int tHdjktvZUy62530729 = -519032129;    int tHdjktvZUy93606587 = -661332868;    int tHdjktvZUy36953948 = -19508029;    int tHdjktvZUy85982976 = -784575923;    int tHdjktvZUy87614145 = -46711376;    int tHdjktvZUy63629324 = 20942232;    int tHdjktvZUy3340880 = -424467840;    int tHdjktvZUy12867274 = -220173175;    int tHdjktvZUy17345767 = -591689713;    int tHdjktvZUy55316963 = -142106728;    int tHdjktvZUy43108645 = -823625750;    int tHdjktvZUy1925186 = -241375533;    int tHdjktvZUy24517061 = -116793616;    int tHdjktvZUy2351827 = -998273976;    int tHdjktvZUy46136529 = -276578862;    int tHdjktvZUy98086279 = -53943324;    int tHdjktvZUy96720706 = -619086351;    int tHdjktvZUy46890906 = -597370742;    int tHdjktvZUy1143191 = -846764912;    int tHdjktvZUy15673543 = -194162136;    int tHdjktvZUy94819892 = -549740865;    int tHdjktvZUy82276535 = -977247393;    int tHdjktvZUy22689877 = -600173813;    int tHdjktvZUy17996192 = -249115865;    int tHdjktvZUy44795362 = -906713821;    int tHdjktvZUy39123213 = -559743022;    int tHdjktvZUy80969859 = -596182361;    int tHdjktvZUy65073282 = -560260229;    int tHdjktvZUy86251579 = -597217369;    int tHdjktvZUy46175064 = -822000178;    int tHdjktvZUy26784120 = 46158623;    int tHdjktvZUy75309754 = -809320391;    int tHdjktvZUy41884878 = -892808681;    int tHdjktvZUy98907143 = -701512466;    int tHdjktvZUy7171914 = 70369862;    int tHdjktvZUy4286339 = -428688011;    int tHdjktvZUy88510336 = -917294535;    int tHdjktvZUy598814 = -75394150;    int tHdjktvZUy27702748 = -963145246;    int tHdjktvZUy23593026 = -291299591;    int tHdjktvZUy68879669 = -967508747;    int tHdjktvZUy63165129 = -384298161;    int tHdjktvZUy18056364 = -770619521;    int tHdjktvZUy20739028 = -876010373;    int tHdjktvZUy79329926 = -880279338;    int tHdjktvZUy3764659 = -291558553;    int tHdjktvZUy56397838 = -18127993;    int tHdjktvZUy84315547 = -348202838;    int tHdjktvZUy61434197 = -852091171;    int tHdjktvZUy96238545 = -395787502;    int tHdjktvZUy88934823 = 55819133;    int tHdjktvZUy71509263 = 99414522;    int tHdjktvZUy48443512 = -637492816;    int tHdjktvZUy83912990 = -791569840;    int tHdjktvZUy9645062 = -223417747;    int tHdjktvZUy80254194 = -541784737;    int tHdjktvZUy70916710 = 38840944;    int tHdjktvZUy18957756 = -770392165;    int tHdjktvZUy41187614 = -877862103;    int tHdjktvZUy48490933 = -486968354;    int tHdjktvZUy82659465 = -382875408;    int tHdjktvZUy38267598 = -707227363;     tHdjktvZUy62207800 = tHdjktvZUy88365541;     tHdjktvZUy88365541 = tHdjktvZUy87894091;     tHdjktvZUy87894091 = tHdjktvZUy70852295;     tHdjktvZUy70852295 = tHdjktvZUy17678666;     tHdjktvZUy17678666 = tHdjktvZUy98745868;     tHdjktvZUy98745868 = tHdjktvZUy40434764;     tHdjktvZUy40434764 = tHdjktvZUy50485206;     tHdjktvZUy50485206 = tHdjktvZUy20038112;     tHdjktvZUy20038112 = tHdjktvZUy67637234;     tHdjktvZUy67637234 = tHdjktvZUy72644008;     tHdjktvZUy72644008 = tHdjktvZUy80576403;     tHdjktvZUy80576403 = tHdjktvZUy31830594;     tHdjktvZUy31830594 = tHdjktvZUy45184290;     tHdjktvZUy45184290 = tHdjktvZUy24896457;     tHdjktvZUy24896457 = tHdjktvZUy41471411;     tHdjktvZUy41471411 = tHdjktvZUy3372098;     tHdjktvZUy3372098 = tHdjktvZUy69702642;     tHdjktvZUy69702642 = tHdjktvZUy97892926;     tHdjktvZUy97892926 = tHdjktvZUy25464284;     tHdjktvZUy25464284 = tHdjktvZUy86581789;     tHdjktvZUy86581789 = tHdjktvZUy15316894;     tHdjktvZUy15316894 = tHdjktvZUy87222350;     tHdjktvZUy87222350 = tHdjktvZUy72220549;     tHdjktvZUy72220549 = tHdjktvZUy76032402;     tHdjktvZUy76032402 = tHdjktvZUy35402131;     tHdjktvZUy35402131 = tHdjktvZUy76055991;     tHdjktvZUy76055991 = tHdjktvZUy22438572;     tHdjktvZUy22438572 = tHdjktvZUy5689844;     tHdjktvZUy5689844 = tHdjktvZUy80914898;     tHdjktvZUy80914898 = tHdjktvZUy86667374;     tHdjktvZUy86667374 = tHdjktvZUy7570727;     tHdjktvZUy7570727 = tHdjktvZUy94324824;     tHdjktvZUy94324824 = tHdjktvZUy85655530;     tHdjktvZUy85655530 = tHdjktvZUy18400170;     tHdjktvZUy18400170 = tHdjktvZUy49586703;     tHdjktvZUy49586703 = tHdjktvZUy99586532;     tHdjktvZUy99586532 = tHdjktvZUy4464955;     tHdjktvZUy4464955 = tHdjktvZUy62530729;     tHdjktvZUy62530729 = tHdjktvZUy93606587;     tHdjktvZUy93606587 = tHdjktvZUy36953948;     tHdjktvZUy36953948 = tHdjktvZUy85982976;     tHdjktvZUy85982976 = tHdjktvZUy87614145;     tHdjktvZUy87614145 = tHdjktvZUy63629324;     tHdjktvZUy63629324 = tHdjktvZUy3340880;     tHdjktvZUy3340880 = tHdjktvZUy12867274;     tHdjktvZUy12867274 = tHdjktvZUy17345767;     tHdjktvZUy17345767 = tHdjktvZUy55316963;     tHdjktvZUy55316963 = tHdjktvZUy43108645;     tHdjktvZUy43108645 = tHdjktvZUy1925186;     tHdjktvZUy1925186 = tHdjktvZUy24517061;     tHdjktvZUy24517061 = tHdjktvZUy2351827;     tHdjktvZUy2351827 = tHdjktvZUy46136529;     tHdjktvZUy46136529 = tHdjktvZUy98086279;     tHdjktvZUy98086279 = tHdjktvZUy96720706;     tHdjktvZUy96720706 = tHdjktvZUy46890906;     tHdjktvZUy46890906 = tHdjktvZUy1143191;     tHdjktvZUy1143191 = tHdjktvZUy15673543;     tHdjktvZUy15673543 = tHdjktvZUy94819892;     tHdjktvZUy94819892 = tHdjktvZUy82276535;     tHdjktvZUy82276535 = tHdjktvZUy22689877;     tHdjktvZUy22689877 = tHdjktvZUy17996192;     tHdjktvZUy17996192 = tHdjktvZUy44795362;     tHdjktvZUy44795362 = tHdjktvZUy39123213;     tHdjktvZUy39123213 = tHdjktvZUy80969859;     tHdjktvZUy80969859 = tHdjktvZUy65073282;     tHdjktvZUy65073282 = tHdjktvZUy86251579;     tHdjktvZUy86251579 = tHdjktvZUy46175064;     tHdjktvZUy46175064 = tHdjktvZUy26784120;     tHdjktvZUy26784120 = tHdjktvZUy75309754;     tHdjktvZUy75309754 = tHdjktvZUy41884878;     tHdjktvZUy41884878 = tHdjktvZUy98907143;     tHdjktvZUy98907143 = tHdjktvZUy7171914;     tHdjktvZUy7171914 = tHdjktvZUy4286339;     tHdjktvZUy4286339 = tHdjktvZUy88510336;     tHdjktvZUy88510336 = tHdjktvZUy598814;     tHdjktvZUy598814 = tHdjktvZUy27702748;     tHdjktvZUy27702748 = tHdjktvZUy23593026;     tHdjktvZUy23593026 = tHdjktvZUy68879669;     tHdjktvZUy68879669 = tHdjktvZUy63165129;     tHdjktvZUy63165129 = tHdjktvZUy18056364;     tHdjktvZUy18056364 = tHdjktvZUy20739028;     tHdjktvZUy20739028 = tHdjktvZUy79329926;     tHdjktvZUy79329926 = tHdjktvZUy3764659;     tHdjktvZUy3764659 = tHdjktvZUy56397838;     tHdjktvZUy56397838 = tHdjktvZUy84315547;     tHdjktvZUy84315547 = tHdjktvZUy61434197;     tHdjktvZUy61434197 = tHdjktvZUy96238545;     tHdjktvZUy96238545 = tHdjktvZUy88934823;     tHdjktvZUy88934823 = tHdjktvZUy71509263;     tHdjktvZUy71509263 = tHdjktvZUy48443512;     tHdjktvZUy48443512 = tHdjktvZUy83912990;     tHdjktvZUy83912990 = tHdjktvZUy9645062;     tHdjktvZUy9645062 = tHdjktvZUy80254194;     tHdjktvZUy80254194 = tHdjktvZUy70916710;     tHdjktvZUy70916710 = tHdjktvZUy18957756;     tHdjktvZUy18957756 = tHdjktvZUy41187614;     tHdjktvZUy41187614 = tHdjktvZUy48490933;     tHdjktvZUy48490933 = tHdjktvZUy82659465;     tHdjktvZUy82659465 = tHdjktvZUy38267598;     tHdjktvZUy38267598 = tHdjktvZUy62207800;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void wJxZwJENrO4238009() {     int lYNvtRNFHd87799548 = -259736322;    int lYNvtRNFHd46190368 = -430004922;    int lYNvtRNFHd19460055 = -531471644;    int lYNvtRNFHd31494365 = -440994263;    int lYNvtRNFHd78180568 = 81876408;    int lYNvtRNFHd80949712 = -73839104;    int lYNvtRNFHd77208272 = -830755692;    int lYNvtRNFHd96281279 = -116152468;    int lYNvtRNFHd62627240 = -577242605;    int lYNvtRNFHd90156102 = -640413826;    int lYNvtRNFHd66656957 = -39886075;    int lYNvtRNFHd1602383 = -855178738;    int lYNvtRNFHd76558457 = -227857286;    int lYNvtRNFHd82899786 = -281168839;    int lYNvtRNFHd35993236 = -219411224;    int lYNvtRNFHd74627906 = 18063518;    int lYNvtRNFHd47317455 = -443376301;    int lYNvtRNFHd67818014 = -667868005;    int lYNvtRNFHd38929377 = -255012470;    int lYNvtRNFHd14551988 = -399217488;    int lYNvtRNFHd7921631 = -611203480;    int lYNvtRNFHd38523735 = 80402332;    int lYNvtRNFHd54107752 = 44508378;    int lYNvtRNFHd30284483 = -369905043;    int lYNvtRNFHd2492853 = -945273624;    int lYNvtRNFHd48317190 = -18553847;    int lYNvtRNFHd88222318 = -383130522;    int lYNvtRNFHd50933847 = -324600332;    int lYNvtRNFHd888610 = -223157621;    int lYNvtRNFHd58147519 = -468399797;    int lYNvtRNFHd75301330 = -838030610;    int lYNvtRNFHd60036802 = -10644105;    int lYNvtRNFHd30059453 = -795607746;    int lYNvtRNFHd38959764 = -842730456;    int lYNvtRNFHd37447120 = 59921793;    int lYNvtRNFHd11550364 = -922086697;    int lYNvtRNFHd37290551 = -366590542;    int lYNvtRNFHd10119962 = -726610757;    int lYNvtRNFHd68140711 = -264689035;    int lYNvtRNFHd30517369 = -421871501;    int lYNvtRNFHd96249560 = -506624321;    int lYNvtRNFHd44294905 = 177655;    int lYNvtRNFHd76531005 = -616187503;    int lYNvtRNFHd75190705 = -890508924;    int lYNvtRNFHd11303704 = -990229556;    int lYNvtRNFHd38839628 = -930676042;    int lYNvtRNFHd89416428 = -142877951;    int lYNvtRNFHd88046153 = -826528219;    int lYNvtRNFHd75261576 = -259538115;    int lYNvtRNFHd35689757 = -502373800;    int lYNvtRNFHd21393938 = -786897212;    int lYNvtRNFHd15063149 = -794207408;    int lYNvtRNFHd88761784 = -952423402;    int lYNvtRNFHd56734917 = -937087254;    int lYNvtRNFHd96316377 = -237277490;    int lYNvtRNFHd49275814 = -240138655;    int lYNvtRNFHd92082616 = -374513300;    int lYNvtRNFHd89175571 = -61566601;    int lYNvtRNFHd29001512 = -495720639;    int lYNvtRNFHd29863378 = -899569745;    int lYNvtRNFHd92727394 = -690708583;    int lYNvtRNFHd26274426 = -406155361;    int lYNvtRNFHd95392670 = -892994847;    int lYNvtRNFHd4479721 = -8842809;    int lYNvtRNFHd14854773 = -802383217;    int lYNvtRNFHd6620156 = 70758029;    int lYNvtRNFHd71542929 = 40429008;    int lYNvtRNFHd37598694 = -385126830;    int lYNvtRNFHd45452666 = -241090633;    int lYNvtRNFHd24442873 = -297324527;    int lYNvtRNFHd37337356 = -615345941;    int lYNvtRNFHd37197493 = -716765544;    int lYNvtRNFHd99677302 = -303178971;    int lYNvtRNFHd8412009 = -833140969;    int lYNvtRNFHd18302427 = -892593168;    int lYNvtRNFHd63626725 = -511381136;    int lYNvtRNFHd61992729 = -303410165;    int lYNvtRNFHd78917046 = -64982699;    int lYNvtRNFHd18980780 = -379675488;    int lYNvtRNFHd63653225 = 85402417;    int lYNvtRNFHd58900762 = -875675896;    int lYNvtRNFHd176166 = -556602304;    int lYNvtRNFHd75672271 = 34937783;    int lYNvtRNFHd65198852 = -720783822;    int lYNvtRNFHd36753582 = -681502586;    int lYNvtRNFHd60238181 = 56176797;    int lYNvtRNFHd71275017 = -58220703;    int lYNvtRNFHd73324535 = -858520493;    int lYNvtRNFHd42643387 = -505452966;    int lYNvtRNFHd88171306 = -699939552;    int lYNvtRNFHd19467748 = -447573397;    int lYNvtRNFHd48114979 = -205023941;    int lYNvtRNFHd81118449 = -130890119;    int lYNvtRNFHd38277333 = -365119290;    int lYNvtRNFHd37789975 = -731162919;    int lYNvtRNFHd69975135 = -468961;    int lYNvtRNFHd48902235 = -106827499;    int lYNvtRNFHd72051284 = -507344695;    int lYNvtRNFHd60335933 = 11874292;    int lYNvtRNFHd4683549 = -259736322;     lYNvtRNFHd87799548 = lYNvtRNFHd46190368;     lYNvtRNFHd46190368 = lYNvtRNFHd19460055;     lYNvtRNFHd19460055 = lYNvtRNFHd31494365;     lYNvtRNFHd31494365 = lYNvtRNFHd78180568;     lYNvtRNFHd78180568 = lYNvtRNFHd80949712;     lYNvtRNFHd80949712 = lYNvtRNFHd77208272;     lYNvtRNFHd77208272 = lYNvtRNFHd96281279;     lYNvtRNFHd96281279 = lYNvtRNFHd62627240;     lYNvtRNFHd62627240 = lYNvtRNFHd90156102;     lYNvtRNFHd90156102 = lYNvtRNFHd66656957;     lYNvtRNFHd66656957 = lYNvtRNFHd1602383;     lYNvtRNFHd1602383 = lYNvtRNFHd76558457;     lYNvtRNFHd76558457 = lYNvtRNFHd82899786;     lYNvtRNFHd82899786 = lYNvtRNFHd35993236;     lYNvtRNFHd35993236 = lYNvtRNFHd74627906;     lYNvtRNFHd74627906 = lYNvtRNFHd47317455;     lYNvtRNFHd47317455 = lYNvtRNFHd67818014;     lYNvtRNFHd67818014 = lYNvtRNFHd38929377;     lYNvtRNFHd38929377 = lYNvtRNFHd14551988;     lYNvtRNFHd14551988 = lYNvtRNFHd7921631;     lYNvtRNFHd7921631 = lYNvtRNFHd38523735;     lYNvtRNFHd38523735 = lYNvtRNFHd54107752;     lYNvtRNFHd54107752 = lYNvtRNFHd30284483;     lYNvtRNFHd30284483 = lYNvtRNFHd2492853;     lYNvtRNFHd2492853 = lYNvtRNFHd48317190;     lYNvtRNFHd48317190 = lYNvtRNFHd88222318;     lYNvtRNFHd88222318 = lYNvtRNFHd50933847;     lYNvtRNFHd50933847 = lYNvtRNFHd888610;     lYNvtRNFHd888610 = lYNvtRNFHd58147519;     lYNvtRNFHd58147519 = lYNvtRNFHd75301330;     lYNvtRNFHd75301330 = lYNvtRNFHd60036802;     lYNvtRNFHd60036802 = lYNvtRNFHd30059453;     lYNvtRNFHd30059453 = lYNvtRNFHd38959764;     lYNvtRNFHd38959764 = lYNvtRNFHd37447120;     lYNvtRNFHd37447120 = lYNvtRNFHd11550364;     lYNvtRNFHd11550364 = lYNvtRNFHd37290551;     lYNvtRNFHd37290551 = lYNvtRNFHd10119962;     lYNvtRNFHd10119962 = lYNvtRNFHd68140711;     lYNvtRNFHd68140711 = lYNvtRNFHd30517369;     lYNvtRNFHd30517369 = lYNvtRNFHd96249560;     lYNvtRNFHd96249560 = lYNvtRNFHd44294905;     lYNvtRNFHd44294905 = lYNvtRNFHd76531005;     lYNvtRNFHd76531005 = lYNvtRNFHd75190705;     lYNvtRNFHd75190705 = lYNvtRNFHd11303704;     lYNvtRNFHd11303704 = lYNvtRNFHd38839628;     lYNvtRNFHd38839628 = lYNvtRNFHd89416428;     lYNvtRNFHd89416428 = lYNvtRNFHd88046153;     lYNvtRNFHd88046153 = lYNvtRNFHd75261576;     lYNvtRNFHd75261576 = lYNvtRNFHd35689757;     lYNvtRNFHd35689757 = lYNvtRNFHd21393938;     lYNvtRNFHd21393938 = lYNvtRNFHd15063149;     lYNvtRNFHd15063149 = lYNvtRNFHd88761784;     lYNvtRNFHd88761784 = lYNvtRNFHd56734917;     lYNvtRNFHd56734917 = lYNvtRNFHd96316377;     lYNvtRNFHd96316377 = lYNvtRNFHd49275814;     lYNvtRNFHd49275814 = lYNvtRNFHd92082616;     lYNvtRNFHd92082616 = lYNvtRNFHd89175571;     lYNvtRNFHd89175571 = lYNvtRNFHd29001512;     lYNvtRNFHd29001512 = lYNvtRNFHd29863378;     lYNvtRNFHd29863378 = lYNvtRNFHd92727394;     lYNvtRNFHd92727394 = lYNvtRNFHd26274426;     lYNvtRNFHd26274426 = lYNvtRNFHd95392670;     lYNvtRNFHd95392670 = lYNvtRNFHd4479721;     lYNvtRNFHd4479721 = lYNvtRNFHd14854773;     lYNvtRNFHd14854773 = lYNvtRNFHd6620156;     lYNvtRNFHd6620156 = lYNvtRNFHd71542929;     lYNvtRNFHd71542929 = lYNvtRNFHd37598694;     lYNvtRNFHd37598694 = lYNvtRNFHd45452666;     lYNvtRNFHd45452666 = lYNvtRNFHd24442873;     lYNvtRNFHd24442873 = lYNvtRNFHd37337356;     lYNvtRNFHd37337356 = lYNvtRNFHd37197493;     lYNvtRNFHd37197493 = lYNvtRNFHd99677302;     lYNvtRNFHd99677302 = lYNvtRNFHd8412009;     lYNvtRNFHd8412009 = lYNvtRNFHd18302427;     lYNvtRNFHd18302427 = lYNvtRNFHd63626725;     lYNvtRNFHd63626725 = lYNvtRNFHd61992729;     lYNvtRNFHd61992729 = lYNvtRNFHd78917046;     lYNvtRNFHd78917046 = lYNvtRNFHd18980780;     lYNvtRNFHd18980780 = lYNvtRNFHd63653225;     lYNvtRNFHd63653225 = lYNvtRNFHd58900762;     lYNvtRNFHd58900762 = lYNvtRNFHd176166;     lYNvtRNFHd176166 = lYNvtRNFHd75672271;     lYNvtRNFHd75672271 = lYNvtRNFHd65198852;     lYNvtRNFHd65198852 = lYNvtRNFHd36753582;     lYNvtRNFHd36753582 = lYNvtRNFHd60238181;     lYNvtRNFHd60238181 = lYNvtRNFHd71275017;     lYNvtRNFHd71275017 = lYNvtRNFHd73324535;     lYNvtRNFHd73324535 = lYNvtRNFHd42643387;     lYNvtRNFHd42643387 = lYNvtRNFHd88171306;     lYNvtRNFHd88171306 = lYNvtRNFHd19467748;     lYNvtRNFHd19467748 = lYNvtRNFHd48114979;     lYNvtRNFHd48114979 = lYNvtRNFHd81118449;     lYNvtRNFHd81118449 = lYNvtRNFHd38277333;     lYNvtRNFHd38277333 = lYNvtRNFHd37789975;     lYNvtRNFHd37789975 = lYNvtRNFHd69975135;     lYNvtRNFHd69975135 = lYNvtRNFHd48902235;     lYNvtRNFHd48902235 = lYNvtRNFHd72051284;     lYNvtRNFHd72051284 = lYNvtRNFHd60335933;     lYNvtRNFHd60335933 = lYNvtRNFHd4683549;     lYNvtRNFHd4683549 = lYNvtRNFHd87799548;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void bVREDmGivP77416052() {     int RhaNRhXnlQ42733158 = -166620293;    int RhaNRhXnlQ73913994 = -813839399;    int RhaNRhXnlQ72370311 = -991638720;    int RhaNRhXnlQ37285008 = -258178767;    int RhaNRhXnlQ26072252 = -181639531;    int RhaNRhXnlQ7959227 = -527238956;    int RhaNRhXnlQ64150221 = -314251189;    int RhaNRhXnlQ54449023 = -150017257;    int RhaNRhXnlQ42091478 = -980119321;    int RhaNRhXnlQ86348024 = -673879999;    int RhaNRhXnlQ38741350 = -315412033;    int RhaNRhXnlQ91373467 = -983123851;    int RhaNRhXnlQ73311848 = -82568593;    int RhaNRhXnlQ90612469 = -454664922;    int RhaNRhXnlQ9542105 = 41792822;    int RhaNRhXnlQ49820309 = -848144373;    int RhaNRhXnlQ53262744 = -222510965;    int RhaNRhXnlQ68640424 = -94979170;    int RhaNRhXnlQ18686317 = -330798385;    int RhaNRhXnlQ16926745 = -931290559;    int RhaNRhXnlQ53795142 = -64224121;    int RhaNRhXnlQ78631076 = -8045292;    int RhaNRhXnlQ55927741 = -531740583;    int RhaNRhXnlQ98148988 = -636353116;    int RhaNRhXnlQ93271603 = -836865079;    int RhaNRhXnlQ47208746 = -584719943;    int RhaNRhXnlQ62349441 = -653765882;    int RhaNRhXnlQ7050422 = -49138797;    int RhaNRhXnlQ76623334 = -165093255;    int RhaNRhXnlQ72297914 = -918914288;    int RhaNRhXnlQ90275967 = -868243955;    int RhaNRhXnlQ52715273 = 43857224;    int RhaNRhXnlQ68301144 = -196467274;    int RhaNRhXnlQ27181861 = -235345743;    int RhaNRhXnlQ60827872 = -794567339;    int RhaNRhXnlQ56348955 = -260454865;    int RhaNRhXnlQ43144450 = -658509279;    int RhaNRhXnlQ32900160 = -791212475;    int RhaNRhXnlQ30531001 = -767298720;    int RhaNRhXnlQ44197744 = -274233458;    int RhaNRhXnlQ83327558 = -697665632;    int RhaNRhXnlQ59821504 = -739471780;    int RhaNRhXnlQ76764264 = -658039833;    int RhaNRhXnlQ39475684 = -186949551;    int RhaNRhXnlQ7330521 = -620604278;    int RhaNRhXnlQ98834114 = -594886425;    int RhaNRhXnlQ63053679 = -59277012;    int RhaNRhXnlQ30544714 = -567580770;    int RhaNRhXnlQ20631444 = -373450373;    int RhaNRhXnlQ81072265 = -67388074;    int RhaNRhXnlQ63340861 = -388983693;    int RhaNRhXnlQ450225 = 96933135;    int RhaNRhXnlQ51759731 = -492523909;    int RhaNRhXnlQ35406522 = -591269539;    int RhaNRhXnlQ23620911 = -601426994;    int RhaNRhXnlQ64102082 = -58575001;    int RhaNRhXnlQ17986253 = -182098817;    int RhaNRhXnlQ74221323 = -255285604;    int RhaNRhXnlQ44013404 = -421313689;    int RhaNRhXnlQ78863505 = -596919588;    int RhaNRhXnlQ45609785 = -873473075;    int RhaNRhXnlQ57099799 = -165112393;    int RhaNRhXnlQ77825689 = -984924003;    int RhaNRhXnlQ69793564 = 38794966;    int RhaNRhXnlQ96072056 = -805636044;    int RhaNRhXnlQ86026076 = -259269257;    int RhaNRhXnlQ23072324 = -686656577;    int RhaNRhXnlQ46129988 = -847222850;    int RhaNRhXnlQ29784598 = -660097584;    int RhaNRhXnlQ53193150 = -697752314;    int RhaNRhXnlQ6675860 = -89635095;    int RhaNRhXnlQ20362585 = -431298491;    int RhaNRhXnlQ38109423 = -327680450;    int RhaNRhXnlQ74488572 = 43435072;    int RhaNRhXnlQ33599187 = -133624927;    int RhaNRhXnlQ93973637 = -324752342;    int RhaNRhXnlQ1866812 = -350005460;    int RhaNRhXnlQ16452058 = -244791032;    int RhaNRhXnlQ90818467 = 84251161;    int RhaNRhXnlQ94437488 = -141978655;    int RhaNRhXnlQ84155067 = -425442932;    int RhaNRhXnlQ31804728 = 13814888;    int RhaNRhXnlQ86418978 = -675688424;    int RhaNRhXnlQ95551068 = 2294819;    int RhaNRhXnlQ8957054 = -429930596;    int RhaNRhXnlQ89825743 = -865177090;    int RhaNRhXnlQ955543 = -463618867;    int RhaNRhXnlQ32894623 = -605197736;    int RhaNRhXnlQ3560951 = -633918750;    int RhaNRhXnlQ96725790 = -635992338;    int RhaNRhXnlQ38362702 = 21643952;    int RhaNRhXnlQ68923127 = -303223675;    int RhaNRhXnlQ88886755 = -269898787;    int RhaNRhXnlQ51667495 = -70379132;    int RhaNRhXnlQ98587959 = -400760383;    int RhaNRhXnlQ26227759 = -432553240;    int RhaNRhXnlQ81995815 = -754547777;    int RhaNRhXnlQ6970701 = -596834799;    int RhaNRhXnlQ43403628 = -381313508;    int RhaNRhXnlQ21304445 = -166620293;     RhaNRhXnlQ42733158 = RhaNRhXnlQ73913994;     RhaNRhXnlQ73913994 = RhaNRhXnlQ72370311;     RhaNRhXnlQ72370311 = RhaNRhXnlQ37285008;     RhaNRhXnlQ37285008 = RhaNRhXnlQ26072252;     RhaNRhXnlQ26072252 = RhaNRhXnlQ7959227;     RhaNRhXnlQ7959227 = RhaNRhXnlQ64150221;     RhaNRhXnlQ64150221 = RhaNRhXnlQ54449023;     RhaNRhXnlQ54449023 = RhaNRhXnlQ42091478;     RhaNRhXnlQ42091478 = RhaNRhXnlQ86348024;     RhaNRhXnlQ86348024 = RhaNRhXnlQ38741350;     RhaNRhXnlQ38741350 = RhaNRhXnlQ91373467;     RhaNRhXnlQ91373467 = RhaNRhXnlQ73311848;     RhaNRhXnlQ73311848 = RhaNRhXnlQ90612469;     RhaNRhXnlQ90612469 = RhaNRhXnlQ9542105;     RhaNRhXnlQ9542105 = RhaNRhXnlQ49820309;     RhaNRhXnlQ49820309 = RhaNRhXnlQ53262744;     RhaNRhXnlQ53262744 = RhaNRhXnlQ68640424;     RhaNRhXnlQ68640424 = RhaNRhXnlQ18686317;     RhaNRhXnlQ18686317 = RhaNRhXnlQ16926745;     RhaNRhXnlQ16926745 = RhaNRhXnlQ53795142;     RhaNRhXnlQ53795142 = RhaNRhXnlQ78631076;     RhaNRhXnlQ78631076 = RhaNRhXnlQ55927741;     RhaNRhXnlQ55927741 = RhaNRhXnlQ98148988;     RhaNRhXnlQ98148988 = RhaNRhXnlQ93271603;     RhaNRhXnlQ93271603 = RhaNRhXnlQ47208746;     RhaNRhXnlQ47208746 = RhaNRhXnlQ62349441;     RhaNRhXnlQ62349441 = RhaNRhXnlQ7050422;     RhaNRhXnlQ7050422 = RhaNRhXnlQ76623334;     RhaNRhXnlQ76623334 = RhaNRhXnlQ72297914;     RhaNRhXnlQ72297914 = RhaNRhXnlQ90275967;     RhaNRhXnlQ90275967 = RhaNRhXnlQ52715273;     RhaNRhXnlQ52715273 = RhaNRhXnlQ68301144;     RhaNRhXnlQ68301144 = RhaNRhXnlQ27181861;     RhaNRhXnlQ27181861 = RhaNRhXnlQ60827872;     RhaNRhXnlQ60827872 = RhaNRhXnlQ56348955;     RhaNRhXnlQ56348955 = RhaNRhXnlQ43144450;     RhaNRhXnlQ43144450 = RhaNRhXnlQ32900160;     RhaNRhXnlQ32900160 = RhaNRhXnlQ30531001;     RhaNRhXnlQ30531001 = RhaNRhXnlQ44197744;     RhaNRhXnlQ44197744 = RhaNRhXnlQ83327558;     RhaNRhXnlQ83327558 = RhaNRhXnlQ59821504;     RhaNRhXnlQ59821504 = RhaNRhXnlQ76764264;     RhaNRhXnlQ76764264 = RhaNRhXnlQ39475684;     RhaNRhXnlQ39475684 = RhaNRhXnlQ7330521;     RhaNRhXnlQ7330521 = RhaNRhXnlQ98834114;     RhaNRhXnlQ98834114 = RhaNRhXnlQ63053679;     RhaNRhXnlQ63053679 = RhaNRhXnlQ30544714;     RhaNRhXnlQ30544714 = RhaNRhXnlQ20631444;     RhaNRhXnlQ20631444 = RhaNRhXnlQ81072265;     RhaNRhXnlQ81072265 = RhaNRhXnlQ63340861;     RhaNRhXnlQ63340861 = RhaNRhXnlQ450225;     RhaNRhXnlQ450225 = RhaNRhXnlQ51759731;     RhaNRhXnlQ51759731 = RhaNRhXnlQ35406522;     RhaNRhXnlQ35406522 = RhaNRhXnlQ23620911;     RhaNRhXnlQ23620911 = RhaNRhXnlQ64102082;     RhaNRhXnlQ64102082 = RhaNRhXnlQ17986253;     RhaNRhXnlQ17986253 = RhaNRhXnlQ74221323;     RhaNRhXnlQ74221323 = RhaNRhXnlQ44013404;     RhaNRhXnlQ44013404 = RhaNRhXnlQ78863505;     RhaNRhXnlQ78863505 = RhaNRhXnlQ45609785;     RhaNRhXnlQ45609785 = RhaNRhXnlQ57099799;     RhaNRhXnlQ57099799 = RhaNRhXnlQ77825689;     RhaNRhXnlQ77825689 = RhaNRhXnlQ69793564;     RhaNRhXnlQ69793564 = RhaNRhXnlQ96072056;     RhaNRhXnlQ96072056 = RhaNRhXnlQ86026076;     RhaNRhXnlQ86026076 = RhaNRhXnlQ23072324;     RhaNRhXnlQ23072324 = RhaNRhXnlQ46129988;     RhaNRhXnlQ46129988 = RhaNRhXnlQ29784598;     RhaNRhXnlQ29784598 = RhaNRhXnlQ53193150;     RhaNRhXnlQ53193150 = RhaNRhXnlQ6675860;     RhaNRhXnlQ6675860 = RhaNRhXnlQ20362585;     RhaNRhXnlQ20362585 = RhaNRhXnlQ38109423;     RhaNRhXnlQ38109423 = RhaNRhXnlQ74488572;     RhaNRhXnlQ74488572 = RhaNRhXnlQ33599187;     RhaNRhXnlQ33599187 = RhaNRhXnlQ93973637;     RhaNRhXnlQ93973637 = RhaNRhXnlQ1866812;     RhaNRhXnlQ1866812 = RhaNRhXnlQ16452058;     RhaNRhXnlQ16452058 = RhaNRhXnlQ90818467;     RhaNRhXnlQ90818467 = RhaNRhXnlQ94437488;     RhaNRhXnlQ94437488 = RhaNRhXnlQ84155067;     RhaNRhXnlQ84155067 = RhaNRhXnlQ31804728;     RhaNRhXnlQ31804728 = RhaNRhXnlQ86418978;     RhaNRhXnlQ86418978 = RhaNRhXnlQ95551068;     RhaNRhXnlQ95551068 = RhaNRhXnlQ8957054;     RhaNRhXnlQ8957054 = RhaNRhXnlQ89825743;     RhaNRhXnlQ89825743 = RhaNRhXnlQ955543;     RhaNRhXnlQ955543 = RhaNRhXnlQ32894623;     RhaNRhXnlQ32894623 = RhaNRhXnlQ3560951;     RhaNRhXnlQ3560951 = RhaNRhXnlQ96725790;     RhaNRhXnlQ96725790 = RhaNRhXnlQ38362702;     RhaNRhXnlQ38362702 = RhaNRhXnlQ68923127;     RhaNRhXnlQ68923127 = RhaNRhXnlQ88886755;     RhaNRhXnlQ88886755 = RhaNRhXnlQ51667495;     RhaNRhXnlQ51667495 = RhaNRhXnlQ98587959;     RhaNRhXnlQ98587959 = RhaNRhXnlQ26227759;     RhaNRhXnlQ26227759 = RhaNRhXnlQ81995815;     RhaNRhXnlQ81995815 = RhaNRhXnlQ6970701;     RhaNRhXnlQ6970701 = RhaNRhXnlQ43403628;     RhaNRhXnlQ43403628 = RhaNRhXnlQ21304445;     RhaNRhXnlQ21304445 = RhaNRhXnlQ42733158;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void dPEkQmESjw98351563() {     int VGokgqfIWs68324906 = -819129252;    int VGokgqfIWs31738821 = 73277949;    int VGokgqfIWs3936276 = -836971642;    int VGokgqfIWs97927076 = -444960830;    int VGokgqfIWs86574154 = -860206497;    int VGokgqfIWs90163070 = -982787148;    int VGokgqfIWs923730 = -91985929;    int VGokgqfIWs245097 = -826521819;    int VGokgqfIWs84680606 = -662697296;    int VGokgqfIWs8866893 = -271634651;    int VGokgqfIWs32754299 = -666367847;    int VGokgqfIWs12399446 = -591354395;    int VGokgqfIWs18039713 = -925158483;    int VGokgqfIWs28327966 = -84036165;    int VGokgqfIWs20638884 = -984040284;    int VGokgqfIWs82976805 = -951540199;    int VGokgqfIWs97208100 = -91216189;    int VGokgqfIWs66755795 = -214184909;    int VGokgqfIWs59722768 = -495789976;    int VGokgqfIWs6014449 = -293705483;    int VGokgqfIWs75134982 = -815457528;    int VGokgqfIWs1837918 = 82213662;    int VGokgqfIWs22813143 = -116874846;    int VGokgqfIWs56212923 = -614281574;    int VGokgqfIWs19732053 = 22332632;    int VGokgqfIWs60123806 = -240964557;    int VGokgqfIWs74515769 = 81220696;    int VGokgqfIWs35545697 = -769834041;    int VGokgqfIWs71822099 = -855316790;    int VGokgqfIWs49530535 = -52392476;    int VGokgqfIWs78909923 = -259797752;    int VGokgqfIWs5181349 = -938116848;    int VGokgqfIWs4035773 = -442344195;    int VGokgqfIWs80486094 = -414808982;    int VGokgqfIWs79874822 = -136689325;    int VGokgqfIWs18312616 = -698283834;    int VGokgqfIWs80848467 = 60632155;    int VGokgqfIWs38555167 = -644664621;    int VGokgqfIWs36140983 = -512955626;    int VGokgqfIWs81108525 = -34772091;    int VGokgqfIWs42623171 = -84781924;    int VGokgqfIWs18133434 = 45281799;    int VGokgqfIWs65681124 = -127515960;    int VGokgqfIWs51037065 = 1599293;    int VGokgqfIWs15293345 = -86365994;    int VGokgqfIWs24806469 = -205389292;    int VGokgqfIWs35124341 = -710465250;    int VGokgqfIWs63273904 = -152002260;    int VGokgqfIWs52784374 = -909362738;    int VGokgqfIWs14836838 = -328386341;    int VGokgqfIWs60217739 = 40912712;    int VGokgqfIWs13161547 = -799000297;    int VGokgqfIWs94384986 = -68368449;    int VGokgqfIWs94055159 = -374413469;    int VGokgqfIWs23216582 = -219618133;    int VGokgqfIWs66486989 = -801342914;    int VGokgqfIWs8925678 = -809847205;    int VGokgqfIWs47723352 = -122690069;    int VGokgqfIWs78195023 = -367293463;    int VGokgqfIWs26450349 = -519241941;    int VGokgqfIWs15647302 = -964007845;    int VGokgqfIWs65378033 = -322151889;    int VGokgqfIWs28422997 = -971205029;    int VGokgqfIWs35150072 = -510304820;    int VGokgqfIWs29956970 = 88163100;    int VGokgqfIWs27572950 = -728251000;    int VGokgqfIWs8363674 = -49010200;    int VGokgqfIWs37553618 = -410349502;    int VGokgqfIWs48453144 = -947346840;    int VGokgqfIWs2326268 = -185756450;    int VGokgqfIWs2128338 = -912172355;    int VGokgqfIWs58652934 = -446551569;    int VGokgqfIWs30614813 = -701229283;    int VGokgqfIWs78614243 = -361017885;    int VGokgqfIWs63391277 = -108923560;    int VGokgqfIWs57001549 = -760739327;    int VGokgqfIWs36156794 = -790270379;    int VGokgqfIWs71776077 = -18474140;    int VGokgqfIWs40919578 = -427915580;    int VGokgqfIWs94925584 = -772278077;    int VGokgqfIWs24999465 = -530499307;    int VGokgqfIWs11241865 = -766777044;    int VGokgqfIWs82761322 = -860471303;    int VGokgqfIWs56985262 = -426930449;    int VGokgqfIWs89312796 = 6694811;    int VGokgqfIWs65748377 = -460797456;    int VGokgqfIWs10796362 = -769748399;    int VGokgqfIWs9980614 = 32069273;    int VGokgqfIWs57269513 = -95190849;    int VGokgqfIWs13387833 = -335346411;    int VGokgqfIWs9386938 = -888436630;    int VGokgqfIWs33125115 = -816677776;    int VGokgqfIWs60360143 = -177371159;    int VGokgqfIWs9690635 = -993713685;    int VGokgqfIWs65461224 = -70764247;    int VGokgqfIWs77245138 = -762630036;    int VGokgqfIWs89710436 = 16486827;    int VGokgqfIWs30531053 = -617211140;    int VGokgqfIWs21080096 = 13436193;    int VGokgqfIWs87720394 = -819129252;     VGokgqfIWs68324906 = VGokgqfIWs31738821;     VGokgqfIWs31738821 = VGokgqfIWs3936276;     VGokgqfIWs3936276 = VGokgqfIWs97927076;     VGokgqfIWs97927076 = VGokgqfIWs86574154;     VGokgqfIWs86574154 = VGokgqfIWs90163070;     VGokgqfIWs90163070 = VGokgqfIWs923730;     VGokgqfIWs923730 = VGokgqfIWs245097;     VGokgqfIWs245097 = VGokgqfIWs84680606;     VGokgqfIWs84680606 = VGokgqfIWs8866893;     VGokgqfIWs8866893 = VGokgqfIWs32754299;     VGokgqfIWs32754299 = VGokgqfIWs12399446;     VGokgqfIWs12399446 = VGokgqfIWs18039713;     VGokgqfIWs18039713 = VGokgqfIWs28327966;     VGokgqfIWs28327966 = VGokgqfIWs20638884;     VGokgqfIWs20638884 = VGokgqfIWs82976805;     VGokgqfIWs82976805 = VGokgqfIWs97208100;     VGokgqfIWs97208100 = VGokgqfIWs66755795;     VGokgqfIWs66755795 = VGokgqfIWs59722768;     VGokgqfIWs59722768 = VGokgqfIWs6014449;     VGokgqfIWs6014449 = VGokgqfIWs75134982;     VGokgqfIWs75134982 = VGokgqfIWs1837918;     VGokgqfIWs1837918 = VGokgqfIWs22813143;     VGokgqfIWs22813143 = VGokgqfIWs56212923;     VGokgqfIWs56212923 = VGokgqfIWs19732053;     VGokgqfIWs19732053 = VGokgqfIWs60123806;     VGokgqfIWs60123806 = VGokgqfIWs74515769;     VGokgqfIWs74515769 = VGokgqfIWs35545697;     VGokgqfIWs35545697 = VGokgqfIWs71822099;     VGokgqfIWs71822099 = VGokgqfIWs49530535;     VGokgqfIWs49530535 = VGokgqfIWs78909923;     VGokgqfIWs78909923 = VGokgqfIWs5181349;     VGokgqfIWs5181349 = VGokgqfIWs4035773;     VGokgqfIWs4035773 = VGokgqfIWs80486094;     VGokgqfIWs80486094 = VGokgqfIWs79874822;     VGokgqfIWs79874822 = VGokgqfIWs18312616;     VGokgqfIWs18312616 = VGokgqfIWs80848467;     VGokgqfIWs80848467 = VGokgqfIWs38555167;     VGokgqfIWs38555167 = VGokgqfIWs36140983;     VGokgqfIWs36140983 = VGokgqfIWs81108525;     VGokgqfIWs81108525 = VGokgqfIWs42623171;     VGokgqfIWs42623171 = VGokgqfIWs18133434;     VGokgqfIWs18133434 = VGokgqfIWs65681124;     VGokgqfIWs65681124 = VGokgqfIWs51037065;     VGokgqfIWs51037065 = VGokgqfIWs15293345;     VGokgqfIWs15293345 = VGokgqfIWs24806469;     VGokgqfIWs24806469 = VGokgqfIWs35124341;     VGokgqfIWs35124341 = VGokgqfIWs63273904;     VGokgqfIWs63273904 = VGokgqfIWs52784374;     VGokgqfIWs52784374 = VGokgqfIWs14836838;     VGokgqfIWs14836838 = VGokgqfIWs60217739;     VGokgqfIWs60217739 = VGokgqfIWs13161547;     VGokgqfIWs13161547 = VGokgqfIWs94384986;     VGokgqfIWs94384986 = VGokgqfIWs94055159;     VGokgqfIWs94055159 = VGokgqfIWs23216582;     VGokgqfIWs23216582 = VGokgqfIWs66486989;     VGokgqfIWs66486989 = VGokgqfIWs8925678;     VGokgqfIWs8925678 = VGokgqfIWs47723352;     VGokgqfIWs47723352 = VGokgqfIWs78195023;     VGokgqfIWs78195023 = VGokgqfIWs26450349;     VGokgqfIWs26450349 = VGokgqfIWs15647302;     VGokgqfIWs15647302 = VGokgqfIWs65378033;     VGokgqfIWs65378033 = VGokgqfIWs28422997;     VGokgqfIWs28422997 = VGokgqfIWs35150072;     VGokgqfIWs35150072 = VGokgqfIWs29956970;     VGokgqfIWs29956970 = VGokgqfIWs27572950;     VGokgqfIWs27572950 = VGokgqfIWs8363674;     VGokgqfIWs8363674 = VGokgqfIWs37553618;     VGokgqfIWs37553618 = VGokgqfIWs48453144;     VGokgqfIWs48453144 = VGokgqfIWs2326268;     VGokgqfIWs2326268 = VGokgqfIWs2128338;     VGokgqfIWs2128338 = VGokgqfIWs58652934;     VGokgqfIWs58652934 = VGokgqfIWs30614813;     VGokgqfIWs30614813 = VGokgqfIWs78614243;     VGokgqfIWs78614243 = VGokgqfIWs63391277;     VGokgqfIWs63391277 = VGokgqfIWs57001549;     VGokgqfIWs57001549 = VGokgqfIWs36156794;     VGokgqfIWs36156794 = VGokgqfIWs71776077;     VGokgqfIWs71776077 = VGokgqfIWs40919578;     VGokgqfIWs40919578 = VGokgqfIWs94925584;     VGokgqfIWs94925584 = VGokgqfIWs24999465;     VGokgqfIWs24999465 = VGokgqfIWs11241865;     VGokgqfIWs11241865 = VGokgqfIWs82761322;     VGokgqfIWs82761322 = VGokgqfIWs56985262;     VGokgqfIWs56985262 = VGokgqfIWs89312796;     VGokgqfIWs89312796 = VGokgqfIWs65748377;     VGokgqfIWs65748377 = VGokgqfIWs10796362;     VGokgqfIWs10796362 = VGokgqfIWs9980614;     VGokgqfIWs9980614 = VGokgqfIWs57269513;     VGokgqfIWs57269513 = VGokgqfIWs13387833;     VGokgqfIWs13387833 = VGokgqfIWs9386938;     VGokgqfIWs9386938 = VGokgqfIWs33125115;     VGokgqfIWs33125115 = VGokgqfIWs60360143;     VGokgqfIWs60360143 = VGokgqfIWs9690635;     VGokgqfIWs9690635 = VGokgqfIWs65461224;     VGokgqfIWs65461224 = VGokgqfIWs77245138;     VGokgqfIWs77245138 = VGokgqfIWs89710436;     VGokgqfIWs89710436 = VGokgqfIWs30531053;     VGokgqfIWs30531053 = VGokgqfIWs21080096;     VGokgqfIWs21080096 = VGokgqfIWs87720394;     VGokgqfIWs87720394 = VGokgqfIWs68324906;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void NYVZMfVmbe71529607() {     int fOxYMHzamY23258517 = -726013222;    int fOxYMHzamY59462446 = -310556528;    int fOxYMHzamY56846532 = -197138719;    int fOxYMHzamY3717720 = -262145334;    int fOxYMHzamY34465838 = -23722437;    int fOxYMHzamY17172585 = -336187000;    int fOxYMHzamY87865678 = -675481426;    int fOxYMHzamY58412840 = -860386608;    int fOxYMHzamY64144845 = 34425988;    int fOxYMHzamY5058815 = -305100824;    int fOxYMHzamY4838692 = -941893804;    int fOxYMHzamY2170531 = -719299509;    int fOxYMHzamY14793103 = -779869790;    int fOxYMHzamY36040650 = -257532248;    int fOxYMHzamY94187752 = -722836239;    int fOxYMHzamY58169207 = -717748090;    int fOxYMHzamY3153391 = -970350854;    int fOxYMHzamY67578205 = -741296074;    int fOxYMHzamY39479707 = -571575891;    int fOxYMHzamY8389206 = -825778554;    int fOxYMHzamY21008494 = -268478169;    int fOxYMHzamY41945259 = -6233962;    int fOxYMHzamY24633132 = -693123807;    int fOxYMHzamY24077429 = -880729647;    int fOxYMHzamY10510804 = -969258823;    int fOxYMHzamY59015362 = -807130653;    int fOxYMHzamY48642892 = -189414663;    int fOxYMHzamY91662271 = -494372506;    int fOxYMHzamY47556824 = -797252423;    int fOxYMHzamY63680930 = -502906967;    int fOxYMHzamY93884561 = -290011097;    int fOxYMHzamY97859820 = -883615519;    int fOxYMHzamY42277464 = -943203724;    int fOxYMHzamY68708190 = -907424269;    int fOxYMHzamY3255574 = -991178457;    int fOxYMHzamY63111207 = -36652002;    int fOxYMHzamY86702367 = -231286582;    int fOxYMHzamY61335365 = -709266339;    int fOxYMHzamY98531272 = 84434689;    int fOxYMHzamY94788900 = -987134047;    int fOxYMHzamY29701169 = -275823235;    int fOxYMHzamY33660032 = -694367636;    int fOxYMHzamY65914383 = -169368289;    int fOxYMHzamY15322044 = -394841334;    int fOxYMHzamY11320163 = -816740717;    int fOxYMHzamY84800956 = -969599674;    int fOxYMHzamY8761592 = -626864312;    int fOxYMHzamY5772465 = -993054812;    int fOxYMHzamY98154241 = 76725004;    int fOxYMHzamY60219346 = -993400616;    int fOxYMHzamY2164662 = -661173769;    int fOxYMHzamY98548621 = 92140246;    int fOxYMHzamY57382933 = -708468956;    int fOxYMHzamY72726763 = -28595754;    int fOxYMHzamY50521114 = -583767636;    int fOxYMHzamY81313257 = -619779261;    int fOxYMHzamY34829315 = -617432722;    int fOxYMHzamY32769104 = -316409072;    int fOxYMHzamY93206915 = -292886512;    int fOxYMHzamY75450475 = -216591784;    int fOxYMHzamY68529692 = -46772337;    int fOxYMHzamY96203406 = -81108921;    int fOxYMHzamY10856017 = 36865815;    int fOxYMHzamY463915 = -462667045;    int fOxYMHzamY11174254 = 84910273;    int fOxYMHzamY6978872 = 41721714;    int fOxYMHzamY59893067 = -776095786;    int fOxYMHzamY46084913 = -872445522;    int fOxYMHzamY32785076 = -266353791;    int fOxYMHzamY31076545 = -586184237;    int fOxYMHzamY71466840 = -386461508;    int fOxYMHzamY41818025 = -161084516;    int fOxYMHzamY69046933 = -725730763;    int fOxYMHzamY44690807 = -584441844;    int fOxYMHzamY78688037 = -449955320;    int fOxYMHzamY87348461 = -574110533;    int fOxYMHzamY76030876 = -836865673;    int fOxYMHzamY9311089 = -198282473;    int fOxYMHzamY12757266 = 36011069;    int fOxYMHzamY25709848 = -999659149;    int fOxYMHzamY50253770 = -80266342;    int fOxYMHzamY42870427 = -196359852;    int fOxYMHzamY93508029 = -471097511;    int fOxYMHzamY87337477 = -803851808;    int fOxYMHzamY61516268 = -841733199;    int fOxYMHzamY95335940 = -282151343;    int fOxYMHzamY40476887 = -75146563;    int fOxYMHzamY69550700 = -814607970;    int fOxYMHzamY18187077 = -223656633;    int fOxYMHzamY21942317 = -271399197;    int fOxYMHzamY28281893 = -419219281;    int fOxYMHzamY53933264 = -914877511;    int fOxYMHzamY68128449 = -316379827;    int fOxYMHzamY23080797 = -698973528;    int fOxYMHzamY26259209 = -840361711;    int fOxYMHzamY33497762 = -94714315;    int fOxYMHzamY22804016 = -631233452;    int fOxYMHzamY65450468 = -706701244;    int fOxYMHzamY4147791 = -379751607;    int fOxYMHzamY4341291 = -726013222;     fOxYMHzamY23258517 = fOxYMHzamY59462446;     fOxYMHzamY59462446 = fOxYMHzamY56846532;     fOxYMHzamY56846532 = fOxYMHzamY3717720;     fOxYMHzamY3717720 = fOxYMHzamY34465838;     fOxYMHzamY34465838 = fOxYMHzamY17172585;     fOxYMHzamY17172585 = fOxYMHzamY87865678;     fOxYMHzamY87865678 = fOxYMHzamY58412840;     fOxYMHzamY58412840 = fOxYMHzamY64144845;     fOxYMHzamY64144845 = fOxYMHzamY5058815;     fOxYMHzamY5058815 = fOxYMHzamY4838692;     fOxYMHzamY4838692 = fOxYMHzamY2170531;     fOxYMHzamY2170531 = fOxYMHzamY14793103;     fOxYMHzamY14793103 = fOxYMHzamY36040650;     fOxYMHzamY36040650 = fOxYMHzamY94187752;     fOxYMHzamY94187752 = fOxYMHzamY58169207;     fOxYMHzamY58169207 = fOxYMHzamY3153391;     fOxYMHzamY3153391 = fOxYMHzamY67578205;     fOxYMHzamY67578205 = fOxYMHzamY39479707;     fOxYMHzamY39479707 = fOxYMHzamY8389206;     fOxYMHzamY8389206 = fOxYMHzamY21008494;     fOxYMHzamY21008494 = fOxYMHzamY41945259;     fOxYMHzamY41945259 = fOxYMHzamY24633132;     fOxYMHzamY24633132 = fOxYMHzamY24077429;     fOxYMHzamY24077429 = fOxYMHzamY10510804;     fOxYMHzamY10510804 = fOxYMHzamY59015362;     fOxYMHzamY59015362 = fOxYMHzamY48642892;     fOxYMHzamY48642892 = fOxYMHzamY91662271;     fOxYMHzamY91662271 = fOxYMHzamY47556824;     fOxYMHzamY47556824 = fOxYMHzamY63680930;     fOxYMHzamY63680930 = fOxYMHzamY93884561;     fOxYMHzamY93884561 = fOxYMHzamY97859820;     fOxYMHzamY97859820 = fOxYMHzamY42277464;     fOxYMHzamY42277464 = fOxYMHzamY68708190;     fOxYMHzamY68708190 = fOxYMHzamY3255574;     fOxYMHzamY3255574 = fOxYMHzamY63111207;     fOxYMHzamY63111207 = fOxYMHzamY86702367;     fOxYMHzamY86702367 = fOxYMHzamY61335365;     fOxYMHzamY61335365 = fOxYMHzamY98531272;     fOxYMHzamY98531272 = fOxYMHzamY94788900;     fOxYMHzamY94788900 = fOxYMHzamY29701169;     fOxYMHzamY29701169 = fOxYMHzamY33660032;     fOxYMHzamY33660032 = fOxYMHzamY65914383;     fOxYMHzamY65914383 = fOxYMHzamY15322044;     fOxYMHzamY15322044 = fOxYMHzamY11320163;     fOxYMHzamY11320163 = fOxYMHzamY84800956;     fOxYMHzamY84800956 = fOxYMHzamY8761592;     fOxYMHzamY8761592 = fOxYMHzamY5772465;     fOxYMHzamY5772465 = fOxYMHzamY98154241;     fOxYMHzamY98154241 = fOxYMHzamY60219346;     fOxYMHzamY60219346 = fOxYMHzamY2164662;     fOxYMHzamY2164662 = fOxYMHzamY98548621;     fOxYMHzamY98548621 = fOxYMHzamY57382933;     fOxYMHzamY57382933 = fOxYMHzamY72726763;     fOxYMHzamY72726763 = fOxYMHzamY50521114;     fOxYMHzamY50521114 = fOxYMHzamY81313257;     fOxYMHzamY81313257 = fOxYMHzamY34829315;     fOxYMHzamY34829315 = fOxYMHzamY32769104;     fOxYMHzamY32769104 = fOxYMHzamY93206915;     fOxYMHzamY93206915 = fOxYMHzamY75450475;     fOxYMHzamY75450475 = fOxYMHzamY68529692;     fOxYMHzamY68529692 = fOxYMHzamY96203406;     fOxYMHzamY96203406 = fOxYMHzamY10856017;     fOxYMHzamY10856017 = fOxYMHzamY463915;     fOxYMHzamY463915 = fOxYMHzamY11174254;     fOxYMHzamY11174254 = fOxYMHzamY6978872;     fOxYMHzamY6978872 = fOxYMHzamY59893067;     fOxYMHzamY59893067 = fOxYMHzamY46084913;     fOxYMHzamY46084913 = fOxYMHzamY32785076;     fOxYMHzamY32785076 = fOxYMHzamY31076545;     fOxYMHzamY31076545 = fOxYMHzamY71466840;     fOxYMHzamY71466840 = fOxYMHzamY41818025;     fOxYMHzamY41818025 = fOxYMHzamY69046933;     fOxYMHzamY69046933 = fOxYMHzamY44690807;     fOxYMHzamY44690807 = fOxYMHzamY78688037;     fOxYMHzamY78688037 = fOxYMHzamY87348461;     fOxYMHzamY87348461 = fOxYMHzamY76030876;     fOxYMHzamY76030876 = fOxYMHzamY9311089;     fOxYMHzamY9311089 = fOxYMHzamY12757266;     fOxYMHzamY12757266 = fOxYMHzamY25709848;     fOxYMHzamY25709848 = fOxYMHzamY50253770;     fOxYMHzamY50253770 = fOxYMHzamY42870427;     fOxYMHzamY42870427 = fOxYMHzamY93508029;     fOxYMHzamY93508029 = fOxYMHzamY87337477;     fOxYMHzamY87337477 = fOxYMHzamY61516268;     fOxYMHzamY61516268 = fOxYMHzamY95335940;     fOxYMHzamY95335940 = fOxYMHzamY40476887;     fOxYMHzamY40476887 = fOxYMHzamY69550700;     fOxYMHzamY69550700 = fOxYMHzamY18187077;     fOxYMHzamY18187077 = fOxYMHzamY21942317;     fOxYMHzamY21942317 = fOxYMHzamY28281893;     fOxYMHzamY28281893 = fOxYMHzamY53933264;     fOxYMHzamY53933264 = fOxYMHzamY68128449;     fOxYMHzamY68128449 = fOxYMHzamY23080797;     fOxYMHzamY23080797 = fOxYMHzamY26259209;     fOxYMHzamY26259209 = fOxYMHzamY33497762;     fOxYMHzamY33497762 = fOxYMHzamY22804016;     fOxYMHzamY22804016 = fOxYMHzamY65450468;     fOxYMHzamY65450468 = fOxYMHzamY4147791;     fOxYMHzamY4147791 = fOxYMHzamY4341291;     fOxYMHzamY4341291 = fOxYMHzamY23258517;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void aYYggBhsKg92465119() {     int aBUjRCtZcd48850265 = -278522181;    int aBUjRCtZcd17287274 = -523439180;    int aBUjRCtZcd88412495 = -42471641;    int aBUjRCtZcd64359788 = -448927398;    int aBUjRCtZcd94967740 = -702289403;    int aBUjRCtZcd99376428 = -791735191;    int aBUjRCtZcd24639187 = -453216166;    int aBUjRCtZcd4208914 = -436891170;    int aBUjRCtZcd6733974 = -748151987;    int aBUjRCtZcd27577684 = 97144524;    int aBUjRCtZcd98851640 = -192849619;    int aBUjRCtZcd23196510 = -327530053;    int aBUjRCtZcd59520967 = -522459681;    int aBUjRCtZcd73756146 = -986903490;    int aBUjRCtZcd5284532 = -648669344;    int aBUjRCtZcd91325703 = -821143916;    int aBUjRCtZcd47098747 = -839056078;    int aBUjRCtZcd65693577 = -860501813;    int aBUjRCtZcd80516158 = -736567482;    int aBUjRCtZcd97476909 = -188193479;    int aBUjRCtZcd42348335 = 80288424;    int aBUjRCtZcd65152100 = 84024992;    int aBUjRCtZcd91518533 = -278258070;    int aBUjRCtZcd82141362 = -858658104;    int aBUjRCtZcd36971253 = -110061112;    int aBUjRCtZcd71930421 = -463375267;    int aBUjRCtZcd60809220 = -554428085;    int aBUjRCtZcd20157547 = -115067750;    int aBUjRCtZcd42755589 = -387475959;    int aBUjRCtZcd40913551 = -736385155;    int aBUjRCtZcd82518517 = -781564893;    int aBUjRCtZcd50325895 = -765589591;    int aBUjRCtZcd78012092 = -89080645;    int aBUjRCtZcd22012425 = 13112492;    int aBUjRCtZcd22302524 = -333300444;    int aBUjRCtZcd25074868 = -474480972;    int aBUjRCtZcd24406385 = -612145148;    int aBUjRCtZcd66990372 = -562718485;    int aBUjRCtZcd4141255 = -761222217;    int aBUjRCtZcd31699682 = -747672680;    int aBUjRCtZcd88996781 = -762939526;    int aBUjRCtZcd91971961 = 90385942;    int aBUjRCtZcd54831243 = -738844416;    int aBUjRCtZcd26883425 = -206292489;    int aBUjRCtZcd19282986 = -282502433;    int aBUjRCtZcd10773310 = -580102541;    int aBUjRCtZcd80832253 = -178052550;    int aBUjRCtZcd38501655 = -577476302;    int aBUjRCtZcd30307173 = -459187361;    int aBUjRCtZcd93983918 = -154398882;    int aBUjRCtZcd99041539 = -231277364;    int aBUjRCtZcd11259944 = -803793185;    int aBUjRCtZcd8189 = -284313497;    int aBUjRCtZcd31375402 = -911739684;    int aBUjRCtZcd50116785 = -201958776;    int aBUjRCtZcd83698164 = -262547174;    int aBUjRCtZcd25768740 = -145181111;    int aBUjRCtZcd6271133 = -183813537;    int aBUjRCtZcd27388535 = -238866286;    int aBUjRCtZcd23037319 = -138914136;    int aBUjRCtZcd38567209 = -137307107;    int aBUjRCtZcd4481641 = -238148416;    int aBUjRCtZcd61453324 = 50584789;    int aBUjRCtZcd65820422 = 88233168;    int aBUjRCtZcd45059166 = -121290583;    int aBUjRCtZcd48525745 = -427260028;    int aBUjRCtZcd45184417 = -138449409;    int aBUjRCtZcd37508543 = -435572174;    int aBUjRCtZcd51453622 = -553603047;    int aBUjRCtZcd80209663 = -74188373;    int aBUjRCtZcd66919318 = -108998768;    int aBUjRCtZcd80108375 = -176337594;    int aBUjRCtZcd61552322 = 720404;    int aBUjRCtZcd48816477 = -988894802;    int aBUjRCtZcd8480128 = -425253953;    int aBUjRCtZcd50376374 = 89902481;    int aBUjRCtZcd10320858 = -177130592;    int aBUjRCtZcd64635108 = 28034419;    int aBUjRCtZcd62858376 = -476155672;    int aBUjRCtZcd26197943 = -529958571;    int aBUjRCtZcd91098168 = -185322717;    int aBUjRCtZcd22307565 = -976951783;    int aBUjRCtZcd89850374 = -655880390;    int aBUjRCtZcd48771671 = -133077077;    int aBUjRCtZcd41872012 = -405107792;    int aBUjRCtZcd71258573 = -977771708;    int aBUjRCtZcd50317707 = -381276095;    int aBUjRCtZcd46636691 = -177340961;    int aBUjRCtZcd71895639 = -784928733;    int aBUjRCtZcd38604359 = 29246729;    int aBUjRCtZcd99306127 = -229299862;    int aBUjRCtZcd18135252 = -328331612;    int aBUjRCtZcd39601837 = -223852199;    int aBUjRCtZcd81103936 = -522308081;    int aBUjRCtZcd93132473 = -510365574;    int aBUjRCtZcd84515141 = -424791111;    int aBUjRCtZcd30518637 = -960198847;    int aBUjRCtZcd89010820 = -727077585;    int aBUjRCtZcd81824258 = 14998094;    int aBUjRCtZcd70757241 = -278522181;     aBUjRCtZcd48850265 = aBUjRCtZcd17287274;     aBUjRCtZcd17287274 = aBUjRCtZcd88412495;     aBUjRCtZcd88412495 = aBUjRCtZcd64359788;     aBUjRCtZcd64359788 = aBUjRCtZcd94967740;     aBUjRCtZcd94967740 = aBUjRCtZcd99376428;     aBUjRCtZcd99376428 = aBUjRCtZcd24639187;     aBUjRCtZcd24639187 = aBUjRCtZcd4208914;     aBUjRCtZcd4208914 = aBUjRCtZcd6733974;     aBUjRCtZcd6733974 = aBUjRCtZcd27577684;     aBUjRCtZcd27577684 = aBUjRCtZcd98851640;     aBUjRCtZcd98851640 = aBUjRCtZcd23196510;     aBUjRCtZcd23196510 = aBUjRCtZcd59520967;     aBUjRCtZcd59520967 = aBUjRCtZcd73756146;     aBUjRCtZcd73756146 = aBUjRCtZcd5284532;     aBUjRCtZcd5284532 = aBUjRCtZcd91325703;     aBUjRCtZcd91325703 = aBUjRCtZcd47098747;     aBUjRCtZcd47098747 = aBUjRCtZcd65693577;     aBUjRCtZcd65693577 = aBUjRCtZcd80516158;     aBUjRCtZcd80516158 = aBUjRCtZcd97476909;     aBUjRCtZcd97476909 = aBUjRCtZcd42348335;     aBUjRCtZcd42348335 = aBUjRCtZcd65152100;     aBUjRCtZcd65152100 = aBUjRCtZcd91518533;     aBUjRCtZcd91518533 = aBUjRCtZcd82141362;     aBUjRCtZcd82141362 = aBUjRCtZcd36971253;     aBUjRCtZcd36971253 = aBUjRCtZcd71930421;     aBUjRCtZcd71930421 = aBUjRCtZcd60809220;     aBUjRCtZcd60809220 = aBUjRCtZcd20157547;     aBUjRCtZcd20157547 = aBUjRCtZcd42755589;     aBUjRCtZcd42755589 = aBUjRCtZcd40913551;     aBUjRCtZcd40913551 = aBUjRCtZcd82518517;     aBUjRCtZcd82518517 = aBUjRCtZcd50325895;     aBUjRCtZcd50325895 = aBUjRCtZcd78012092;     aBUjRCtZcd78012092 = aBUjRCtZcd22012425;     aBUjRCtZcd22012425 = aBUjRCtZcd22302524;     aBUjRCtZcd22302524 = aBUjRCtZcd25074868;     aBUjRCtZcd25074868 = aBUjRCtZcd24406385;     aBUjRCtZcd24406385 = aBUjRCtZcd66990372;     aBUjRCtZcd66990372 = aBUjRCtZcd4141255;     aBUjRCtZcd4141255 = aBUjRCtZcd31699682;     aBUjRCtZcd31699682 = aBUjRCtZcd88996781;     aBUjRCtZcd88996781 = aBUjRCtZcd91971961;     aBUjRCtZcd91971961 = aBUjRCtZcd54831243;     aBUjRCtZcd54831243 = aBUjRCtZcd26883425;     aBUjRCtZcd26883425 = aBUjRCtZcd19282986;     aBUjRCtZcd19282986 = aBUjRCtZcd10773310;     aBUjRCtZcd10773310 = aBUjRCtZcd80832253;     aBUjRCtZcd80832253 = aBUjRCtZcd38501655;     aBUjRCtZcd38501655 = aBUjRCtZcd30307173;     aBUjRCtZcd30307173 = aBUjRCtZcd93983918;     aBUjRCtZcd93983918 = aBUjRCtZcd99041539;     aBUjRCtZcd99041539 = aBUjRCtZcd11259944;     aBUjRCtZcd11259944 = aBUjRCtZcd8189;     aBUjRCtZcd8189 = aBUjRCtZcd31375402;     aBUjRCtZcd31375402 = aBUjRCtZcd50116785;     aBUjRCtZcd50116785 = aBUjRCtZcd83698164;     aBUjRCtZcd83698164 = aBUjRCtZcd25768740;     aBUjRCtZcd25768740 = aBUjRCtZcd6271133;     aBUjRCtZcd6271133 = aBUjRCtZcd27388535;     aBUjRCtZcd27388535 = aBUjRCtZcd23037319;     aBUjRCtZcd23037319 = aBUjRCtZcd38567209;     aBUjRCtZcd38567209 = aBUjRCtZcd4481641;     aBUjRCtZcd4481641 = aBUjRCtZcd61453324;     aBUjRCtZcd61453324 = aBUjRCtZcd65820422;     aBUjRCtZcd65820422 = aBUjRCtZcd45059166;     aBUjRCtZcd45059166 = aBUjRCtZcd48525745;     aBUjRCtZcd48525745 = aBUjRCtZcd45184417;     aBUjRCtZcd45184417 = aBUjRCtZcd37508543;     aBUjRCtZcd37508543 = aBUjRCtZcd51453622;     aBUjRCtZcd51453622 = aBUjRCtZcd80209663;     aBUjRCtZcd80209663 = aBUjRCtZcd66919318;     aBUjRCtZcd66919318 = aBUjRCtZcd80108375;     aBUjRCtZcd80108375 = aBUjRCtZcd61552322;     aBUjRCtZcd61552322 = aBUjRCtZcd48816477;     aBUjRCtZcd48816477 = aBUjRCtZcd8480128;     aBUjRCtZcd8480128 = aBUjRCtZcd50376374;     aBUjRCtZcd50376374 = aBUjRCtZcd10320858;     aBUjRCtZcd10320858 = aBUjRCtZcd64635108;     aBUjRCtZcd64635108 = aBUjRCtZcd62858376;     aBUjRCtZcd62858376 = aBUjRCtZcd26197943;     aBUjRCtZcd26197943 = aBUjRCtZcd91098168;     aBUjRCtZcd91098168 = aBUjRCtZcd22307565;     aBUjRCtZcd22307565 = aBUjRCtZcd89850374;     aBUjRCtZcd89850374 = aBUjRCtZcd48771671;     aBUjRCtZcd48771671 = aBUjRCtZcd41872012;     aBUjRCtZcd41872012 = aBUjRCtZcd71258573;     aBUjRCtZcd71258573 = aBUjRCtZcd50317707;     aBUjRCtZcd50317707 = aBUjRCtZcd46636691;     aBUjRCtZcd46636691 = aBUjRCtZcd71895639;     aBUjRCtZcd71895639 = aBUjRCtZcd38604359;     aBUjRCtZcd38604359 = aBUjRCtZcd99306127;     aBUjRCtZcd99306127 = aBUjRCtZcd18135252;     aBUjRCtZcd18135252 = aBUjRCtZcd39601837;     aBUjRCtZcd39601837 = aBUjRCtZcd81103936;     aBUjRCtZcd81103936 = aBUjRCtZcd93132473;     aBUjRCtZcd93132473 = aBUjRCtZcd84515141;     aBUjRCtZcd84515141 = aBUjRCtZcd30518637;     aBUjRCtZcd30518637 = aBUjRCtZcd89010820;     aBUjRCtZcd89010820 = aBUjRCtZcd81824258;     aBUjRCtZcd81824258 = aBUjRCtZcd70757241;     aBUjRCtZcd70757241 = aBUjRCtZcd48850265;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void QKbgEoYppj65643163() {     int JqgSVuqUOW3783875 = -185406152;    int JqgSVuqUOW45010899 = -907273658;    int JqgSVuqUOW41322752 = -502638717;    int JqgSVuqUOW70150431 = -266111902;    int JqgSVuqUOW42859423 = -965805342;    int JqgSVuqUOW26385943 = -145135043;    int JqgSVuqUOW11581136 = 63288337;    int JqgSVuqUOW62376657 = -470755959;    int JqgSVuqUOW86198212 = -51028703;    int JqgSVuqUOW23769605 = 63678351;    int JqgSVuqUOW70936033 = -468375576;    int JqgSVuqUOW12967595 = -455475167;    int JqgSVuqUOW56274358 = -377170988;    int JqgSVuqUOW81468829 = -60399574;    int JqgSVuqUOW78833400 = -387465299;    int JqgSVuqUOW66518106 = -587351806;    int JqgSVuqUOW53044037 = -618190742;    int JqgSVuqUOW66515987 = -287612977;    int JqgSVuqUOW60273098 = -812353397;    int JqgSVuqUOW99851666 = -720266549;    int JqgSVuqUOW88221845 = -472732217;    int JqgSVuqUOW5259442 = -4422632;    int JqgSVuqUOW93338522 = -854507030;    int JqgSVuqUOW50005868 = -25106178;    int JqgSVuqUOW27750004 = -1652566;    int JqgSVuqUOW70821977 = 70458637;    int JqgSVuqUOW34936343 = -825063444;    int JqgSVuqUOW76274122 = -939606215;    int JqgSVuqUOW18490314 = -329411592;    int JqgSVuqUOW55063946 = -86899646;    int JqgSVuqUOW97493155 = -811778239;    int JqgSVuqUOW43004367 = -711088262;    int JqgSVuqUOW16253784 = -589940173;    int JqgSVuqUOW10234521 = -479502796;    int JqgSVuqUOW45683276 = -87789576;    int JqgSVuqUOW69873459 = -912849140;    int JqgSVuqUOW30260284 = -904063885;    int JqgSVuqUOW89770570 = -627320202;    int JqgSVuqUOW66531544 = -163831902;    int JqgSVuqUOW45380057 = -600034637;    int JqgSVuqUOW76074779 = -953980838;    int JqgSVuqUOW7498560 = -649263492;    int JqgSVuqUOW55064502 = -780696746;    int JqgSVuqUOW91168403 = -602733117;    int JqgSVuqUOW15309804 = 87122845;    int JqgSVuqUOW70767797 = -244312924;    int JqgSVuqUOW54469504 = -94451611;    int JqgSVuqUOW81000216 = -318528854;    int JqgSVuqUOW75677040 = -573099618;    int JqgSVuqUOW39366427 = -819413157;    int JqgSVuqUOW40988463 = -933363845;    int JqgSVuqUOW96647018 = 87347357;    int JqgSVuqUOW63006135 = -924414003;    int JqgSVuqUOW10047006 = -565921969;    int JqgSVuqUOW77421317 = -566108279;    int JqgSVuqUOW98524432 = -80983520;    int JqgSVuqUOW51672377 = 47233372;    int JqgSVuqUOW91316883 = -377532540;    int JqgSVuqUOW42400427 = -164459336;    int JqgSVuqUOW72037446 = -936263980;    int JqgSVuqUOW91449599 = -320071599;    int JqgSVuqUOW35307014 = 2894552;    int JqgSVuqUOW43886344 = -41344367;    int JqgSVuqUOW31134266 = -964129057;    int JqgSVuqUOW26276450 = -124543411;    int JqgSVuqUOW27931667 = -757287315;    int JqgSVuqUOW96713811 = -865534994;    int JqgSVuqUOW46039837 = -897668193;    int JqgSVuqUOW35785554 = -972609998;    int JqgSVuqUOW8959941 = -474616159;    int JqgSVuqUOW36257822 = -683287921;    int JqgSVuqUOW63273466 = -990870541;    int JqgSVuqUOW99984442 = -23781076;    int JqgSVuqUOW14893041 = -112318761;    int JqgSVuqUOW23776888 = -766285712;    int JqgSVuqUOW80723285 = -823468725;    int JqgSVuqUOW50194940 = -223725887;    int JqgSVuqUOW2170120 = -151773914;    int JqgSVuqUOW34696065 = -12229023;    int JqgSVuqUOW56982207 = -757339643;    int JqgSVuqUOW16352473 = -835089753;    int JqgSVuqUOW53936127 = -406534591;    int JqgSVuqUOW597082 = -266506597;    int JqgSVuqUOW79123887 = -509998436;    int JqgSVuqUOW14075484 = -153535802;    int JqgSVuqUOW846137 = -799125596;    int JqgSVuqUOW79998231 = -786674259;    int JqgSVuqUOW6206778 = 75981795;    int JqgSVuqUOW32813203 = -913394517;    int JqgSVuqUOW47158843 = 93193944;    int JqgSVuqUOW18201083 = -860082513;    int JqgSVuqUOW38943400 = -426531346;    int JqgSVuqUOW47370143 = -362860867;    int JqgSVuqUOW94494098 = -227567923;    int JqgSVuqUOW53930458 = -179963038;    int JqgSVuqUOW40767765 = -856875390;    int JqgSVuqUOW63612216 = -507919126;    int JqgSVuqUOW23930237 = -816567690;    int JqgSVuqUOW64891953 = -378189706;    int JqgSVuqUOW87378136 = -185406152;     JqgSVuqUOW3783875 = JqgSVuqUOW45010899;     JqgSVuqUOW45010899 = JqgSVuqUOW41322752;     JqgSVuqUOW41322752 = JqgSVuqUOW70150431;     JqgSVuqUOW70150431 = JqgSVuqUOW42859423;     JqgSVuqUOW42859423 = JqgSVuqUOW26385943;     JqgSVuqUOW26385943 = JqgSVuqUOW11581136;     JqgSVuqUOW11581136 = JqgSVuqUOW62376657;     JqgSVuqUOW62376657 = JqgSVuqUOW86198212;     JqgSVuqUOW86198212 = JqgSVuqUOW23769605;     JqgSVuqUOW23769605 = JqgSVuqUOW70936033;     JqgSVuqUOW70936033 = JqgSVuqUOW12967595;     JqgSVuqUOW12967595 = JqgSVuqUOW56274358;     JqgSVuqUOW56274358 = JqgSVuqUOW81468829;     JqgSVuqUOW81468829 = JqgSVuqUOW78833400;     JqgSVuqUOW78833400 = JqgSVuqUOW66518106;     JqgSVuqUOW66518106 = JqgSVuqUOW53044037;     JqgSVuqUOW53044037 = JqgSVuqUOW66515987;     JqgSVuqUOW66515987 = JqgSVuqUOW60273098;     JqgSVuqUOW60273098 = JqgSVuqUOW99851666;     JqgSVuqUOW99851666 = JqgSVuqUOW88221845;     JqgSVuqUOW88221845 = JqgSVuqUOW5259442;     JqgSVuqUOW5259442 = JqgSVuqUOW93338522;     JqgSVuqUOW93338522 = JqgSVuqUOW50005868;     JqgSVuqUOW50005868 = JqgSVuqUOW27750004;     JqgSVuqUOW27750004 = JqgSVuqUOW70821977;     JqgSVuqUOW70821977 = JqgSVuqUOW34936343;     JqgSVuqUOW34936343 = JqgSVuqUOW76274122;     JqgSVuqUOW76274122 = JqgSVuqUOW18490314;     JqgSVuqUOW18490314 = JqgSVuqUOW55063946;     JqgSVuqUOW55063946 = JqgSVuqUOW97493155;     JqgSVuqUOW97493155 = JqgSVuqUOW43004367;     JqgSVuqUOW43004367 = JqgSVuqUOW16253784;     JqgSVuqUOW16253784 = JqgSVuqUOW10234521;     JqgSVuqUOW10234521 = JqgSVuqUOW45683276;     JqgSVuqUOW45683276 = JqgSVuqUOW69873459;     JqgSVuqUOW69873459 = JqgSVuqUOW30260284;     JqgSVuqUOW30260284 = JqgSVuqUOW89770570;     JqgSVuqUOW89770570 = JqgSVuqUOW66531544;     JqgSVuqUOW66531544 = JqgSVuqUOW45380057;     JqgSVuqUOW45380057 = JqgSVuqUOW76074779;     JqgSVuqUOW76074779 = JqgSVuqUOW7498560;     JqgSVuqUOW7498560 = JqgSVuqUOW55064502;     JqgSVuqUOW55064502 = JqgSVuqUOW91168403;     JqgSVuqUOW91168403 = JqgSVuqUOW15309804;     JqgSVuqUOW15309804 = JqgSVuqUOW70767797;     JqgSVuqUOW70767797 = JqgSVuqUOW54469504;     JqgSVuqUOW54469504 = JqgSVuqUOW81000216;     JqgSVuqUOW81000216 = JqgSVuqUOW75677040;     JqgSVuqUOW75677040 = JqgSVuqUOW39366427;     JqgSVuqUOW39366427 = JqgSVuqUOW40988463;     JqgSVuqUOW40988463 = JqgSVuqUOW96647018;     JqgSVuqUOW96647018 = JqgSVuqUOW63006135;     JqgSVuqUOW63006135 = JqgSVuqUOW10047006;     JqgSVuqUOW10047006 = JqgSVuqUOW77421317;     JqgSVuqUOW77421317 = JqgSVuqUOW98524432;     JqgSVuqUOW98524432 = JqgSVuqUOW51672377;     JqgSVuqUOW51672377 = JqgSVuqUOW91316883;     JqgSVuqUOW91316883 = JqgSVuqUOW42400427;     JqgSVuqUOW42400427 = JqgSVuqUOW72037446;     JqgSVuqUOW72037446 = JqgSVuqUOW91449599;     JqgSVuqUOW91449599 = JqgSVuqUOW35307014;     JqgSVuqUOW35307014 = JqgSVuqUOW43886344;     JqgSVuqUOW43886344 = JqgSVuqUOW31134266;     JqgSVuqUOW31134266 = JqgSVuqUOW26276450;     JqgSVuqUOW26276450 = JqgSVuqUOW27931667;     JqgSVuqUOW27931667 = JqgSVuqUOW96713811;     JqgSVuqUOW96713811 = JqgSVuqUOW46039837;     JqgSVuqUOW46039837 = JqgSVuqUOW35785554;     JqgSVuqUOW35785554 = JqgSVuqUOW8959941;     JqgSVuqUOW8959941 = JqgSVuqUOW36257822;     JqgSVuqUOW36257822 = JqgSVuqUOW63273466;     JqgSVuqUOW63273466 = JqgSVuqUOW99984442;     JqgSVuqUOW99984442 = JqgSVuqUOW14893041;     JqgSVuqUOW14893041 = JqgSVuqUOW23776888;     JqgSVuqUOW23776888 = JqgSVuqUOW80723285;     JqgSVuqUOW80723285 = JqgSVuqUOW50194940;     JqgSVuqUOW50194940 = JqgSVuqUOW2170120;     JqgSVuqUOW2170120 = JqgSVuqUOW34696065;     JqgSVuqUOW34696065 = JqgSVuqUOW56982207;     JqgSVuqUOW56982207 = JqgSVuqUOW16352473;     JqgSVuqUOW16352473 = JqgSVuqUOW53936127;     JqgSVuqUOW53936127 = JqgSVuqUOW597082;     JqgSVuqUOW597082 = JqgSVuqUOW79123887;     JqgSVuqUOW79123887 = JqgSVuqUOW14075484;     JqgSVuqUOW14075484 = JqgSVuqUOW846137;     JqgSVuqUOW846137 = JqgSVuqUOW79998231;     JqgSVuqUOW79998231 = JqgSVuqUOW6206778;     JqgSVuqUOW6206778 = JqgSVuqUOW32813203;     JqgSVuqUOW32813203 = JqgSVuqUOW47158843;     JqgSVuqUOW47158843 = JqgSVuqUOW18201083;     JqgSVuqUOW18201083 = JqgSVuqUOW38943400;     JqgSVuqUOW38943400 = JqgSVuqUOW47370143;     JqgSVuqUOW47370143 = JqgSVuqUOW94494098;     JqgSVuqUOW94494098 = JqgSVuqUOW53930458;     JqgSVuqUOW53930458 = JqgSVuqUOW40767765;     JqgSVuqUOW40767765 = JqgSVuqUOW63612216;     JqgSVuqUOW63612216 = JqgSVuqUOW23930237;     JqgSVuqUOW23930237 = JqgSVuqUOW64891953;     JqgSVuqUOW64891953 = JqgSVuqUOW87378136;     JqgSVuqUOW87378136 = JqgSVuqUOW3783875;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void RoxdqFoEDL86578675() {     int HQsVyYDuNr29375624 = -837915111;    int HQsVyYDuNr2835726 = -20156309;    int HQsVyYDuNr72888715 = -347971640;    int HQsVyYDuNr30792500 = -452893965;    int HQsVyYDuNr3361326 = -544372308;    int HQsVyYDuNr8589787 = -600683235;    int HQsVyYDuNr48354645 = -814446403;    int HQsVyYDuNr8172732 = -47260521;    int HQsVyYDuNr28787341 = -833606677;    int HQsVyYDuNr46288474 = -634076301;    int HQsVyYDuNr64948982 = -819331391;    int HQsVyYDuNr33993574 = -63705711;    int HQsVyYDuNr1002222 = -119760879;    int HQsVyYDuNr19184326 = -789770816;    int HQsVyYDuNr89930179 = -313298405;    int HQsVyYDuNr99674601 = -690747633;    int HQsVyYDuNr96989393 = -486895966;    int HQsVyYDuNr64631358 = -406818716;    int HQsVyYDuNr1309550 = -977344988;    int HQsVyYDuNr88939370 = -82681474;    int HQsVyYDuNr9561687 = -123965624;    int HQsVyYDuNr28466283 = 85836322;    int HQsVyYDuNr60223923 = -439641293;    int HQsVyYDuNr8069803 = -3034635;    int HQsVyYDuNr54210454 = -242454856;    int HQsVyYDuNr83737037 = -685785977;    int HQsVyYDuNr47102670 = -90076866;    int HQsVyYDuNr4769398 = -560301460;    int HQsVyYDuNr13689080 = 80364872;    int HQsVyYDuNr32296567 = -320377835;    int HQsVyYDuNr86127110 = -203332035;    int HQsVyYDuNr95470442 = -593062335;    int HQsVyYDuNr51988412 = -835817094;    int HQsVyYDuNr63538755 = -658966034;    int HQsVyYDuNr64730226 = -529911563;    int HQsVyYDuNr31837121 = -250678110;    int HQsVyYDuNr67964301 = -184922452;    int HQsVyYDuNr95425577 = -480772348;    int HQsVyYDuNr72141526 = 90511192;    int HQsVyYDuNr82290838 = -360573270;    int HQsVyYDuNr35370393 = -341097129;    int HQsVyYDuNr65810489 = -964509914;    int HQsVyYDuNr43981362 = -250172873;    int HQsVyYDuNr2729784 = -414184272;    int HQsVyYDuNr23272627 = -478638871;    int HQsVyYDuNr96740151 = -954815791;    int HQsVyYDuNr26540166 = -745639849;    int HQsVyYDuNr13729406 = 97049656;    int HQsVyYDuNr7829971 = -9011984;    int HQsVyYDuNr73130999 = 19588576;    int HQsVyYDuNr37865340 = -503467440;    int HQsVyYDuNr9358341 = -808586074;    int HQsVyYDuNr5631391 = -500258544;    int HQsVyYDuNr68695644 = -349065899;    int HQsVyYDuNr77016988 = -184299419;    int HQsVyYDuNr909341 = -823751433;    int HQsVyYDuNr42611802 = -580515016;    int HQsVyYDuNr64818913 = -244937005;    int HQsVyYDuNr76582046 = -110439110;    int HQsVyYDuNr19624289 = -858586332;    int HQsVyYDuNr61487116 = -410606369;    int HQsVyYDuNr43585248 = -154144944;    int HQsVyYDuNr94483651 = -27625394;    int HQsVyYDuNr96490773 = -413228843;    int HQsVyYDuNr60161363 = -330744267;    int HQsVyYDuNr69478540 = -126269057;    int HQsVyYDuNr82005161 = -227888617;    int HQsVyYDuNr37463467 = -460794845;    int HQsVyYDuNr54454100 = -159859254;    int HQsVyYDuNr58093058 = 37379705;    int HQsVyYDuNr31710300 = -405825182;    int HQsVyYDuNr1563816 = 93876381;    int HQsVyYDuNr92489832 = -397329909;    int HQsVyYDuNr19018711 = -516771719;    int HQsVyYDuNr53568978 = -741584345;    int HQsVyYDuNr43751198 = -159455711;    int HQsVyYDuNr84484921 = -663990806;    int HQsVyYDuNr57494139 = 74542978;    int HQsVyYDuNr84797175 = -524395764;    int HQsVyYDuNr57470302 = -287639066;    int HQsVyYDuNr57196871 = -940146128;    int HQsVyYDuNr33373265 = -87126522;    int HQsVyYDuNr96939426 = -451289476;    int HQsVyYDuNr40558081 = -939223705;    int HQsVyYDuNr94431226 = -816910395;    int HQsVyYDuNr76768770 = -394745961;    int HQsVyYDuNr89839051 = 7196209;    int HQsVyYDuNr83292768 = -386751196;    int HQsVyYDuNr86521766 = -374666616;    int HQsVyYDuNr63820885 = -706160130;    int HQsVyYDuNr89225318 = -670163094;    int HQsVyYDuNr3145389 = -939985447;    int HQsVyYDuNr18843531 = -270333239;    int HQsVyYDuNr52517237 = -50902476;    int HQsVyYDuNr20803723 = -949966901;    int HQsVyYDuNr91785145 = -86952186;    int HQsVyYDuNr71326837 = -836884521;    int HQsVyYDuNr47490588 = -836944030;    int HQsVyYDuNr42568421 = 16559994;    int HQsVyYDuNr53794087 = -837915111;     HQsVyYDuNr29375624 = HQsVyYDuNr2835726;     HQsVyYDuNr2835726 = HQsVyYDuNr72888715;     HQsVyYDuNr72888715 = HQsVyYDuNr30792500;     HQsVyYDuNr30792500 = HQsVyYDuNr3361326;     HQsVyYDuNr3361326 = HQsVyYDuNr8589787;     HQsVyYDuNr8589787 = HQsVyYDuNr48354645;     HQsVyYDuNr48354645 = HQsVyYDuNr8172732;     HQsVyYDuNr8172732 = HQsVyYDuNr28787341;     HQsVyYDuNr28787341 = HQsVyYDuNr46288474;     HQsVyYDuNr46288474 = HQsVyYDuNr64948982;     HQsVyYDuNr64948982 = HQsVyYDuNr33993574;     HQsVyYDuNr33993574 = HQsVyYDuNr1002222;     HQsVyYDuNr1002222 = HQsVyYDuNr19184326;     HQsVyYDuNr19184326 = HQsVyYDuNr89930179;     HQsVyYDuNr89930179 = HQsVyYDuNr99674601;     HQsVyYDuNr99674601 = HQsVyYDuNr96989393;     HQsVyYDuNr96989393 = HQsVyYDuNr64631358;     HQsVyYDuNr64631358 = HQsVyYDuNr1309550;     HQsVyYDuNr1309550 = HQsVyYDuNr88939370;     HQsVyYDuNr88939370 = HQsVyYDuNr9561687;     HQsVyYDuNr9561687 = HQsVyYDuNr28466283;     HQsVyYDuNr28466283 = HQsVyYDuNr60223923;     HQsVyYDuNr60223923 = HQsVyYDuNr8069803;     HQsVyYDuNr8069803 = HQsVyYDuNr54210454;     HQsVyYDuNr54210454 = HQsVyYDuNr83737037;     HQsVyYDuNr83737037 = HQsVyYDuNr47102670;     HQsVyYDuNr47102670 = HQsVyYDuNr4769398;     HQsVyYDuNr4769398 = HQsVyYDuNr13689080;     HQsVyYDuNr13689080 = HQsVyYDuNr32296567;     HQsVyYDuNr32296567 = HQsVyYDuNr86127110;     HQsVyYDuNr86127110 = HQsVyYDuNr95470442;     HQsVyYDuNr95470442 = HQsVyYDuNr51988412;     HQsVyYDuNr51988412 = HQsVyYDuNr63538755;     HQsVyYDuNr63538755 = HQsVyYDuNr64730226;     HQsVyYDuNr64730226 = HQsVyYDuNr31837121;     HQsVyYDuNr31837121 = HQsVyYDuNr67964301;     HQsVyYDuNr67964301 = HQsVyYDuNr95425577;     HQsVyYDuNr95425577 = HQsVyYDuNr72141526;     HQsVyYDuNr72141526 = HQsVyYDuNr82290838;     HQsVyYDuNr82290838 = HQsVyYDuNr35370393;     HQsVyYDuNr35370393 = HQsVyYDuNr65810489;     HQsVyYDuNr65810489 = HQsVyYDuNr43981362;     HQsVyYDuNr43981362 = HQsVyYDuNr2729784;     HQsVyYDuNr2729784 = HQsVyYDuNr23272627;     HQsVyYDuNr23272627 = HQsVyYDuNr96740151;     HQsVyYDuNr96740151 = HQsVyYDuNr26540166;     HQsVyYDuNr26540166 = HQsVyYDuNr13729406;     HQsVyYDuNr13729406 = HQsVyYDuNr7829971;     HQsVyYDuNr7829971 = HQsVyYDuNr73130999;     HQsVyYDuNr73130999 = HQsVyYDuNr37865340;     HQsVyYDuNr37865340 = HQsVyYDuNr9358341;     HQsVyYDuNr9358341 = HQsVyYDuNr5631391;     HQsVyYDuNr5631391 = HQsVyYDuNr68695644;     HQsVyYDuNr68695644 = HQsVyYDuNr77016988;     HQsVyYDuNr77016988 = HQsVyYDuNr909341;     HQsVyYDuNr909341 = HQsVyYDuNr42611802;     HQsVyYDuNr42611802 = HQsVyYDuNr64818913;     HQsVyYDuNr64818913 = HQsVyYDuNr76582046;     HQsVyYDuNr76582046 = HQsVyYDuNr19624289;     HQsVyYDuNr19624289 = HQsVyYDuNr61487116;     HQsVyYDuNr61487116 = HQsVyYDuNr43585248;     HQsVyYDuNr43585248 = HQsVyYDuNr94483651;     HQsVyYDuNr94483651 = HQsVyYDuNr96490773;     HQsVyYDuNr96490773 = HQsVyYDuNr60161363;     HQsVyYDuNr60161363 = HQsVyYDuNr69478540;     HQsVyYDuNr69478540 = HQsVyYDuNr82005161;     HQsVyYDuNr82005161 = HQsVyYDuNr37463467;     HQsVyYDuNr37463467 = HQsVyYDuNr54454100;     HQsVyYDuNr54454100 = HQsVyYDuNr58093058;     HQsVyYDuNr58093058 = HQsVyYDuNr31710300;     HQsVyYDuNr31710300 = HQsVyYDuNr1563816;     HQsVyYDuNr1563816 = HQsVyYDuNr92489832;     HQsVyYDuNr92489832 = HQsVyYDuNr19018711;     HQsVyYDuNr19018711 = HQsVyYDuNr53568978;     HQsVyYDuNr53568978 = HQsVyYDuNr43751198;     HQsVyYDuNr43751198 = HQsVyYDuNr84484921;     HQsVyYDuNr84484921 = HQsVyYDuNr57494139;     HQsVyYDuNr57494139 = HQsVyYDuNr84797175;     HQsVyYDuNr84797175 = HQsVyYDuNr57470302;     HQsVyYDuNr57470302 = HQsVyYDuNr57196871;     HQsVyYDuNr57196871 = HQsVyYDuNr33373265;     HQsVyYDuNr33373265 = HQsVyYDuNr96939426;     HQsVyYDuNr96939426 = HQsVyYDuNr40558081;     HQsVyYDuNr40558081 = HQsVyYDuNr94431226;     HQsVyYDuNr94431226 = HQsVyYDuNr76768770;     HQsVyYDuNr76768770 = HQsVyYDuNr89839051;     HQsVyYDuNr89839051 = HQsVyYDuNr83292768;     HQsVyYDuNr83292768 = HQsVyYDuNr86521766;     HQsVyYDuNr86521766 = HQsVyYDuNr63820885;     HQsVyYDuNr63820885 = HQsVyYDuNr89225318;     HQsVyYDuNr89225318 = HQsVyYDuNr3145389;     HQsVyYDuNr3145389 = HQsVyYDuNr18843531;     HQsVyYDuNr18843531 = HQsVyYDuNr52517237;     HQsVyYDuNr52517237 = HQsVyYDuNr20803723;     HQsVyYDuNr20803723 = HQsVyYDuNr91785145;     HQsVyYDuNr91785145 = HQsVyYDuNr71326837;     HQsVyYDuNr71326837 = HQsVyYDuNr47490588;     HQsVyYDuNr47490588 = HQsVyYDuNr42568421;     HQsVyYDuNr42568421 = HQsVyYDuNr53794087;     HQsVyYDuNr53794087 = HQsVyYDuNr29375624;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void lvcCbdoAFQ59756719() {     int URuPDSiZcS84309233 = -744799081;    int URuPDSiZcS30559352 = -403990787;    int URuPDSiZcS25798972 = -808138716;    int URuPDSiZcS36583143 = -270078469;    int URuPDSiZcS51253009 = -807888248;    int URuPDSiZcS35599301 = 45916913;    int URuPDSiZcS35296594 = -297941900;    int URuPDSiZcS66340475 = -81125310;    int URuPDSiZcS8251579 = -136483394;    int URuPDSiZcS42480396 = -667542474;    int URuPDSiZcS37033375 = 5142652;    int URuPDSiZcS23764658 = -191650825;    int URuPDSiZcS97755612 = 25527814;    int URuPDSiZcS26897010 = -963266900;    int URuPDSiZcS63479047 = -52094360;    int URuPDSiZcS74867004 = -456955523;    int URuPDSiZcS2934683 = -266030631;    int URuPDSiZcS65453768 = -933929881;    int URuPDSiZcS81066488 = 46869097;    int URuPDSiZcS91314128 = -614754544;    int URuPDSiZcS55435198 = -676986265;    int URuPDSiZcS68573625 = -2611302;    int URuPDSiZcS62043913 = 84109746;    int URuPDSiZcS75934307 = -269482708;    int URuPDSiZcS44989204 = -134046310;    int URuPDSiZcS82628593 = -151952073;    int URuPDSiZcS21229794 = -360712226;    int URuPDSiZcS60885972 = -284839925;    int URuPDSiZcS89423804 = -961570761;    int URuPDSiZcS46446962 = -770892326;    int URuPDSiZcS1101749 = -233545380;    int URuPDSiZcS88148913 = -538561006;    int URuPDSiZcS90230103 = -236676622;    int URuPDSiZcS51760851 = -51581322;    int URuPDSiZcS88110977 = -284400695;    int URuPDSiZcS76635712 = -689046278;    int URuPDSiZcS73818201 = -476841189;    int URuPDSiZcS18205776 = -545374066;    int URuPDSiZcS34531816 = -412098493;    int URuPDSiZcS95971214 = -212935227;    int URuPDSiZcS22448390 = -532138441;    int URuPDSiZcS81337088 = -604159349;    int URuPDSiZcS44214621 = -292025202;    int URuPDSiZcS67014762 = -810624899;    int URuPDSiZcS19299445 = -109013594;    int URuPDSiZcS56734638 = -619026173;    int URuPDSiZcS177418 = -662038910;    int URuPDSiZcS56227967 = -744002896;    int URuPDSiZcS53199839 = -122924241;    int URuPDSiZcS18513508 = -645425698;    int URuPDSiZcS79812263 = -105553921;    int URuPDSiZcS94745416 = 82554468;    int URuPDSiZcS68629337 = -40359051;    int URuPDSiZcS47367248 = -3248184;    int URuPDSiZcS4321522 = -548448922;    int URuPDSiZcS15735609 = -642187780;    int URuPDSiZcS68515439 = -388100533;    int URuPDSiZcS49864664 = -438656008;    int URuPDSiZcS91593938 = -36032159;    int URuPDSiZcS68624416 = -555936175;    int URuPDSiZcS14369507 = -593370862;    int URuPDSiZcS74410621 = 86898024;    int URuPDSiZcS76916671 = -119554549;    int URuPDSiZcS61804617 = -365591068;    int URuPDSiZcS41378647 = -333997094;    int URuPDSiZcS48884462 = -456296343;    int URuPDSiZcS33534555 = -954974203;    int URuPDSiZcS45994761 = -922890865;    int URuPDSiZcS38786032 = -578866205;    int URuPDSiZcS86843335 = -363048082;    int URuPDSiZcS1048803 = -980114335;    int URuPDSiZcS84728907 = -720656565;    int URuPDSiZcS30921953 = -421831388;    int URuPDSiZcS85095274 = -740195677;    int URuPDSiZcS68865738 = 17383896;    int URuPDSiZcS74098110 = 27173083;    int URuPDSiZcS24359004 = -710586100;    int URuPDSiZcS95029150 = -105265355;    int URuPDSiZcS56634863 = -60469115;    int URuPDSiZcS88254566 = -515020137;    int URuPDSiZcS82451176 = -489913164;    int URuPDSiZcS65001827 = -616709330;    int URuPDSiZcS7686134 = -61915684;    int URuPDSiZcS70910296 = -216145064;    int URuPDSiZcS66634699 = -565338405;    int URuPDSiZcS6356333 = -216099849;    int URuPDSiZcS19519577 = -398201956;    int URuPDSiZcS42862855 = -133428439;    int URuPDSiZcS47439330 = -503132400;    int URuPDSiZcS72375369 = -642212916;    int URuPDSiZcS8120273 = -200945745;    int URuPDSiZcS23953537 = 61814819;    int URuPDSiZcS26611837 = -409341907;    int URuPDSiZcS65907400 = -856162319;    int URuPDSiZcS81601707 = -619564365;    int URuPDSiZcS48037768 = -519036465;    int URuPDSiZcS4420418 = -384604800;    int URuPDSiZcS82410004 = -926434135;    int URuPDSiZcS25636116 = -376627805;    int URuPDSiZcS70414983 = -744799081;     URuPDSiZcS84309233 = URuPDSiZcS30559352;     URuPDSiZcS30559352 = URuPDSiZcS25798972;     URuPDSiZcS25798972 = URuPDSiZcS36583143;     URuPDSiZcS36583143 = URuPDSiZcS51253009;     URuPDSiZcS51253009 = URuPDSiZcS35599301;     URuPDSiZcS35599301 = URuPDSiZcS35296594;     URuPDSiZcS35296594 = URuPDSiZcS66340475;     URuPDSiZcS66340475 = URuPDSiZcS8251579;     URuPDSiZcS8251579 = URuPDSiZcS42480396;     URuPDSiZcS42480396 = URuPDSiZcS37033375;     URuPDSiZcS37033375 = URuPDSiZcS23764658;     URuPDSiZcS23764658 = URuPDSiZcS97755612;     URuPDSiZcS97755612 = URuPDSiZcS26897010;     URuPDSiZcS26897010 = URuPDSiZcS63479047;     URuPDSiZcS63479047 = URuPDSiZcS74867004;     URuPDSiZcS74867004 = URuPDSiZcS2934683;     URuPDSiZcS2934683 = URuPDSiZcS65453768;     URuPDSiZcS65453768 = URuPDSiZcS81066488;     URuPDSiZcS81066488 = URuPDSiZcS91314128;     URuPDSiZcS91314128 = URuPDSiZcS55435198;     URuPDSiZcS55435198 = URuPDSiZcS68573625;     URuPDSiZcS68573625 = URuPDSiZcS62043913;     URuPDSiZcS62043913 = URuPDSiZcS75934307;     URuPDSiZcS75934307 = URuPDSiZcS44989204;     URuPDSiZcS44989204 = URuPDSiZcS82628593;     URuPDSiZcS82628593 = URuPDSiZcS21229794;     URuPDSiZcS21229794 = URuPDSiZcS60885972;     URuPDSiZcS60885972 = URuPDSiZcS89423804;     URuPDSiZcS89423804 = URuPDSiZcS46446962;     URuPDSiZcS46446962 = URuPDSiZcS1101749;     URuPDSiZcS1101749 = URuPDSiZcS88148913;     URuPDSiZcS88148913 = URuPDSiZcS90230103;     URuPDSiZcS90230103 = URuPDSiZcS51760851;     URuPDSiZcS51760851 = URuPDSiZcS88110977;     URuPDSiZcS88110977 = URuPDSiZcS76635712;     URuPDSiZcS76635712 = URuPDSiZcS73818201;     URuPDSiZcS73818201 = URuPDSiZcS18205776;     URuPDSiZcS18205776 = URuPDSiZcS34531816;     URuPDSiZcS34531816 = URuPDSiZcS95971214;     URuPDSiZcS95971214 = URuPDSiZcS22448390;     URuPDSiZcS22448390 = URuPDSiZcS81337088;     URuPDSiZcS81337088 = URuPDSiZcS44214621;     URuPDSiZcS44214621 = URuPDSiZcS67014762;     URuPDSiZcS67014762 = URuPDSiZcS19299445;     URuPDSiZcS19299445 = URuPDSiZcS56734638;     URuPDSiZcS56734638 = URuPDSiZcS177418;     URuPDSiZcS177418 = URuPDSiZcS56227967;     URuPDSiZcS56227967 = URuPDSiZcS53199839;     URuPDSiZcS53199839 = URuPDSiZcS18513508;     URuPDSiZcS18513508 = URuPDSiZcS79812263;     URuPDSiZcS79812263 = URuPDSiZcS94745416;     URuPDSiZcS94745416 = URuPDSiZcS68629337;     URuPDSiZcS68629337 = URuPDSiZcS47367248;     URuPDSiZcS47367248 = URuPDSiZcS4321522;     URuPDSiZcS4321522 = URuPDSiZcS15735609;     URuPDSiZcS15735609 = URuPDSiZcS68515439;     URuPDSiZcS68515439 = URuPDSiZcS49864664;     URuPDSiZcS49864664 = URuPDSiZcS91593938;     URuPDSiZcS91593938 = URuPDSiZcS68624416;     URuPDSiZcS68624416 = URuPDSiZcS14369507;     URuPDSiZcS14369507 = URuPDSiZcS74410621;     URuPDSiZcS74410621 = URuPDSiZcS76916671;     URuPDSiZcS76916671 = URuPDSiZcS61804617;     URuPDSiZcS61804617 = URuPDSiZcS41378647;     URuPDSiZcS41378647 = URuPDSiZcS48884462;     URuPDSiZcS48884462 = URuPDSiZcS33534555;     URuPDSiZcS33534555 = URuPDSiZcS45994761;     URuPDSiZcS45994761 = URuPDSiZcS38786032;     URuPDSiZcS38786032 = URuPDSiZcS86843335;     URuPDSiZcS86843335 = URuPDSiZcS1048803;     URuPDSiZcS1048803 = URuPDSiZcS84728907;     URuPDSiZcS84728907 = URuPDSiZcS30921953;     URuPDSiZcS30921953 = URuPDSiZcS85095274;     URuPDSiZcS85095274 = URuPDSiZcS68865738;     URuPDSiZcS68865738 = URuPDSiZcS74098110;     URuPDSiZcS74098110 = URuPDSiZcS24359004;     URuPDSiZcS24359004 = URuPDSiZcS95029150;     URuPDSiZcS95029150 = URuPDSiZcS56634863;     URuPDSiZcS56634863 = URuPDSiZcS88254566;     URuPDSiZcS88254566 = URuPDSiZcS82451176;     URuPDSiZcS82451176 = URuPDSiZcS65001827;     URuPDSiZcS65001827 = URuPDSiZcS7686134;     URuPDSiZcS7686134 = URuPDSiZcS70910296;     URuPDSiZcS70910296 = URuPDSiZcS66634699;     URuPDSiZcS66634699 = URuPDSiZcS6356333;     URuPDSiZcS6356333 = URuPDSiZcS19519577;     URuPDSiZcS19519577 = URuPDSiZcS42862855;     URuPDSiZcS42862855 = URuPDSiZcS47439330;     URuPDSiZcS47439330 = URuPDSiZcS72375369;     URuPDSiZcS72375369 = URuPDSiZcS8120273;     URuPDSiZcS8120273 = URuPDSiZcS23953537;     URuPDSiZcS23953537 = URuPDSiZcS26611837;     URuPDSiZcS26611837 = URuPDSiZcS65907400;     URuPDSiZcS65907400 = URuPDSiZcS81601707;     URuPDSiZcS81601707 = URuPDSiZcS48037768;     URuPDSiZcS48037768 = URuPDSiZcS4420418;     URuPDSiZcS4420418 = URuPDSiZcS82410004;     URuPDSiZcS82410004 = URuPDSiZcS25636116;     URuPDSiZcS25636116 = URuPDSiZcS70414983;     URuPDSiZcS70414983 = URuPDSiZcS84309233;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void WVmXMkfZHn80692231() {     int hmpuTNrxrA9900982 = -297308040;    int hmpuTNrxrA88384178 = -616873438;    int hmpuTNrxrA57364935 = -653471638;    int hmpuTNrxrA97225212 = -456860532;    int hmpuTNrxrA11754912 = -386455214;    int hmpuTNrxrA17803145 = -409631278;    int hmpuTNrxrA72070102 = -75676640;    int hmpuTNrxrA12136549 = -757629872;    int hmpuTNrxrA50840707 = -919061368;    int hmpuTNrxrA64999264 = -265297127;    int hmpuTNrxrA31046324 = -345813163;    int hmpuTNrxrA44790637 = -899881369;    int hmpuTNrxrA42483476 = -817062077;    int hmpuTNrxrA64612506 = -592638142;    int hmpuTNrxrA74575826 = 22072535;    int hmpuTNrxrA8023500 = -560351349;    int hmpuTNrxrA46880040 = -134735855;    int hmpuTNrxrA63569140 = 46864380;    int hmpuTNrxrA22102940 = -118122494;    int hmpuTNrxrA80401831 = 22830531;    int hmpuTNrxrA76775039 = -328219673;    int hmpuTNrxrA91780465 = 87647652;    int hmpuTNrxrA28929314 = -601024517;    int hmpuTNrxrA33998242 = -247411166;    int hmpuTNrxrA71449654 = -374848599;    int hmpuTNrxrA95543652 = -908196687;    int hmpuTNrxrA33396121 = -725725647;    int hmpuTNrxrA89381247 = 94464831;    int hmpuTNrxrA84622569 = -551794297;    int hmpuTNrxrA23679583 = 95629486;    int hmpuTNrxrA89735704 = -725099177;    int hmpuTNrxrA40614989 = -420535078;    int hmpuTNrxrA25964732 = -482553543;    int hmpuTNrxrA5065085 = -231044561;    int hmpuTNrxrA7157928 = -726522681;    int hmpuTNrxrA38599373 = -26875248;    int hmpuTNrxrA11522219 = -857699755;    int hmpuTNrxrA23860783 = -398826212;    int hmpuTNrxrA40141798 = -157755399;    int hmpuTNrxrA32881995 = 26526140;    int hmpuTNrxrA81744003 = 80745268;    int hmpuTNrxrA39649017 = -919405771;    int hmpuTNrxrA33131481 = -861501330;    int hmpuTNrxrA78576143 = -622076055;    int hmpuTNrxrA27262269 = -674775310;    int hmpuTNrxrA82706992 = -229529040;    int hmpuTNrxrA72248078 = -213227149;    int hmpuTNrxrA88957156 = -328424386;    int hmpuTNrxrA85352769 = -658836606;    int hmpuTNrxrA52278080 = -906423965;    int hmpuTNrxrA76689141 = -775657516;    int hmpuTNrxrA7456739 = -813378963;    int hmpuTNrxrA11254593 = -716203591;    int hmpuTNrxrA6015887 = -886392114;    int hmpuTNrxrA3917193 = -166640061;    int hmpuTNrxrA18120516 = -284955693;    int hmpuTNrxrA59454864 = 84151078;    int hmpuTNrxrA23366694 = -306060473;    int hmpuTNrxrA25775558 = 17988067;    int hmpuTNrxrA16211259 = -478258528;    int hmpuTNrxrA84407023 = -683905631;    int hmpuTNrxrA82688854 = -70141472;    int hmpuTNrxrA27513979 = -105835576;    int hmpuTNrxrA27161125 = -914690855;    int hmpuTNrxrA75263560 = -540197950;    int hmpuTNrxrA90431335 = -925278085;    int hmpuTNrxrA18825905 = -317327826;    int hmpuTNrxrA37418391 = -486017517;    int hmpuTNrxrA57454578 = -866115461;    int hmpuTNrxrA35976454 = -951052218;    int hmpuTNrxrA96501280 = -702651595;    int hmpuTNrxrA23019257 = -735909644;    int hmpuTNrxrA23427342 = -795380222;    int hmpuTNrxrA89220944 = -44648635;    int hmpuTNrxrA98657828 = 42085263;    int hmpuTNrxrA37126022 = -408813903;    int hmpuTNrxrA58648985 = -50851019;    int hmpuTNrxrA50353171 = -978948463;    int hmpuTNrxrA6735974 = -572635856;    int hmpuTNrxrA88742661 = -45319560;    int hmpuTNrxrA23295574 = -594969539;    int hmpuTNrxrA44438964 = -297301262;    int hmpuTNrxrA4028478 = -246698563;    int hmpuTNrxrA32344490 = -645370333;    int hmpuTNrxrA46990442 = -128712998;    int hmpuTNrxrA82278966 = -911720214;    int hmpuTNrxrA29360396 = -704331487;    int hmpuTNrxrA19948846 = -596161430;    int hmpuTNrxrA1147893 = 35595500;    int hmpuTNrxrA89037411 = -341566989;    int hmpuTNrxrA79144508 = -11026327;    int hmpuTNrxrA88155525 = -451639282;    int hmpuTNrxrA98085224 = -316814279;    int hmpuTNrxrA23930539 = -679496872;    int hmpuTNrxrA48474972 = -289568229;    int hmpuTNrxrA99055148 = -849113261;    int hmpuTNrxrA12135038 = -713570195;    int hmpuTNrxrA5970356 = -946810475;    int hmpuTNrxrA3312584 = 18121895;    int hmpuTNrxrA36830933 = -297308040;     hmpuTNrxrA9900982 = hmpuTNrxrA88384178;     hmpuTNrxrA88384178 = hmpuTNrxrA57364935;     hmpuTNrxrA57364935 = hmpuTNrxrA97225212;     hmpuTNrxrA97225212 = hmpuTNrxrA11754912;     hmpuTNrxrA11754912 = hmpuTNrxrA17803145;     hmpuTNrxrA17803145 = hmpuTNrxrA72070102;     hmpuTNrxrA72070102 = hmpuTNrxrA12136549;     hmpuTNrxrA12136549 = hmpuTNrxrA50840707;     hmpuTNrxrA50840707 = hmpuTNrxrA64999264;     hmpuTNrxrA64999264 = hmpuTNrxrA31046324;     hmpuTNrxrA31046324 = hmpuTNrxrA44790637;     hmpuTNrxrA44790637 = hmpuTNrxrA42483476;     hmpuTNrxrA42483476 = hmpuTNrxrA64612506;     hmpuTNrxrA64612506 = hmpuTNrxrA74575826;     hmpuTNrxrA74575826 = hmpuTNrxrA8023500;     hmpuTNrxrA8023500 = hmpuTNrxrA46880040;     hmpuTNrxrA46880040 = hmpuTNrxrA63569140;     hmpuTNrxrA63569140 = hmpuTNrxrA22102940;     hmpuTNrxrA22102940 = hmpuTNrxrA80401831;     hmpuTNrxrA80401831 = hmpuTNrxrA76775039;     hmpuTNrxrA76775039 = hmpuTNrxrA91780465;     hmpuTNrxrA91780465 = hmpuTNrxrA28929314;     hmpuTNrxrA28929314 = hmpuTNrxrA33998242;     hmpuTNrxrA33998242 = hmpuTNrxrA71449654;     hmpuTNrxrA71449654 = hmpuTNrxrA95543652;     hmpuTNrxrA95543652 = hmpuTNrxrA33396121;     hmpuTNrxrA33396121 = hmpuTNrxrA89381247;     hmpuTNrxrA89381247 = hmpuTNrxrA84622569;     hmpuTNrxrA84622569 = hmpuTNrxrA23679583;     hmpuTNrxrA23679583 = hmpuTNrxrA89735704;     hmpuTNrxrA89735704 = hmpuTNrxrA40614989;     hmpuTNrxrA40614989 = hmpuTNrxrA25964732;     hmpuTNrxrA25964732 = hmpuTNrxrA5065085;     hmpuTNrxrA5065085 = hmpuTNrxrA7157928;     hmpuTNrxrA7157928 = hmpuTNrxrA38599373;     hmpuTNrxrA38599373 = hmpuTNrxrA11522219;     hmpuTNrxrA11522219 = hmpuTNrxrA23860783;     hmpuTNrxrA23860783 = hmpuTNrxrA40141798;     hmpuTNrxrA40141798 = hmpuTNrxrA32881995;     hmpuTNrxrA32881995 = hmpuTNrxrA81744003;     hmpuTNrxrA81744003 = hmpuTNrxrA39649017;     hmpuTNrxrA39649017 = hmpuTNrxrA33131481;     hmpuTNrxrA33131481 = hmpuTNrxrA78576143;     hmpuTNrxrA78576143 = hmpuTNrxrA27262269;     hmpuTNrxrA27262269 = hmpuTNrxrA82706992;     hmpuTNrxrA82706992 = hmpuTNrxrA72248078;     hmpuTNrxrA72248078 = hmpuTNrxrA88957156;     hmpuTNrxrA88957156 = hmpuTNrxrA85352769;     hmpuTNrxrA85352769 = hmpuTNrxrA52278080;     hmpuTNrxrA52278080 = hmpuTNrxrA76689141;     hmpuTNrxrA76689141 = hmpuTNrxrA7456739;     hmpuTNrxrA7456739 = hmpuTNrxrA11254593;     hmpuTNrxrA11254593 = hmpuTNrxrA6015887;     hmpuTNrxrA6015887 = hmpuTNrxrA3917193;     hmpuTNrxrA3917193 = hmpuTNrxrA18120516;     hmpuTNrxrA18120516 = hmpuTNrxrA59454864;     hmpuTNrxrA59454864 = hmpuTNrxrA23366694;     hmpuTNrxrA23366694 = hmpuTNrxrA25775558;     hmpuTNrxrA25775558 = hmpuTNrxrA16211259;     hmpuTNrxrA16211259 = hmpuTNrxrA84407023;     hmpuTNrxrA84407023 = hmpuTNrxrA82688854;     hmpuTNrxrA82688854 = hmpuTNrxrA27513979;     hmpuTNrxrA27513979 = hmpuTNrxrA27161125;     hmpuTNrxrA27161125 = hmpuTNrxrA75263560;     hmpuTNrxrA75263560 = hmpuTNrxrA90431335;     hmpuTNrxrA90431335 = hmpuTNrxrA18825905;     hmpuTNrxrA18825905 = hmpuTNrxrA37418391;     hmpuTNrxrA37418391 = hmpuTNrxrA57454578;     hmpuTNrxrA57454578 = hmpuTNrxrA35976454;     hmpuTNrxrA35976454 = hmpuTNrxrA96501280;     hmpuTNrxrA96501280 = hmpuTNrxrA23019257;     hmpuTNrxrA23019257 = hmpuTNrxrA23427342;     hmpuTNrxrA23427342 = hmpuTNrxrA89220944;     hmpuTNrxrA89220944 = hmpuTNrxrA98657828;     hmpuTNrxrA98657828 = hmpuTNrxrA37126022;     hmpuTNrxrA37126022 = hmpuTNrxrA58648985;     hmpuTNrxrA58648985 = hmpuTNrxrA50353171;     hmpuTNrxrA50353171 = hmpuTNrxrA6735974;     hmpuTNrxrA6735974 = hmpuTNrxrA88742661;     hmpuTNrxrA88742661 = hmpuTNrxrA23295574;     hmpuTNrxrA23295574 = hmpuTNrxrA44438964;     hmpuTNrxrA44438964 = hmpuTNrxrA4028478;     hmpuTNrxrA4028478 = hmpuTNrxrA32344490;     hmpuTNrxrA32344490 = hmpuTNrxrA46990442;     hmpuTNrxrA46990442 = hmpuTNrxrA82278966;     hmpuTNrxrA82278966 = hmpuTNrxrA29360396;     hmpuTNrxrA29360396 = hmpuTNrxrA19948846;     hmpuTNrxrA19948846 = hmpuTNrxrA1147893;     hmpuTNrxrA1147893 = hmpuTNrxrA89037411;     hmpuTNrxrA89037411 = hmpuTNrxrA79144508;     hmpuTNrxrA79144508 = hmpuTNrxrA88155525;     hmpuTNrxrA88155525 = hmpuTNrxrA98085224;     hmpuTNrxrA98085224 = hmpuTNrxrA23930539;     hmpuTNrxrA23930539 = hmpuTNrxrA48474972;     hmpuTNrxrA48474972 = hmpuTNrxrA99055148;     hmpuTNrxrA99055148 = hmpuTNrxrA12135038;     hmpuTNrxrA12135038 = hmpuTNrxrA5970356;     hmpuTNrxrA5970356 = hmpuTNrxrA3312584;     hmpuTNrxrA3312584 = hmpuTNrxrA36830933;     hmpuTNrxrA36830933 = hmpuTNrxrA9900982;}
// Junk Finished
