#include "StdAfx.h"
#include "UITalkWnd.h"
#include "UITalkDialogWnd.h"

#include "../Actor.h"
#include "../ai/stalker/ai_stalker.h"
#include "../stalker_movement_manager_smart_cover.h"
#include "../sight_manager.h"
#include "../trade.h"
#include "UIGameSP.h"
#include "../PDA.h"
#include "../../xrServerEntities/character_info.h"
#include "../Level.h"

#include "../PhraseDialog.h"
#include "../PhraseDialogManager.h"

#include "../game_cl_base.h"
#include "../../xrEngine/string_table.h"
#include "../../xrEngine/CameraBase.h"
#include "../../xrEngine/xr_ioc_cmd.h"
#include "../../xrEngine/XR_IOConsole.h"
#include "../../xrUI/UIXmlInit.h"
#include "../../xrUI/Widgets/UI3tButton.h"

#include "GamePersistent.h"

bool EnableTalkDof = true;

CUITalkWnd::CUITalkWnd()
{
	m_pActor				= nullptr;

	m_pOurInvOwner			= nullptr;
	m_pOthersInvOwner		= nullptr;
	m_pOurDialogManager		= nullptr;
	m_pOthersDialogManager	= nullptr;

	ToTopicMode				();

	const static Fvector4 talkDof = EngineExternal().GetTalkDof();
	m_TalkDof.set(talkDof);

	const static float fovScale = EngineExternal().GetTalkFovScale();
	m_talkFovScale = fovScale;
	clamp(m_talkFovScale, 0.2f, 1.0f);

	InitTalkWnd				();
	m_bNeedToUpdateQuestions = false;
	b_disable_break			= false;
	
	ActionRepeaters()->Register(this, kUI_UP);
	ActionRepeaters()->Register(this, kUI_DOWN);
	ActionRepeaters()->Register(this, kUI_SECONDARY_UP, 1, 50);
	ActionRepeaters()->Register(this, kUI_SECONDARY_DOWN, 1, 50);
}

CUITalkWnd::~CUITalkWnd()
{
	RestoreTalkEffects();
}

void CUITalkWnd::InitTalkWnd()
{
	inherited::SetWndRect(Frect().set(0, 0, UI_BASE_WIDTH, UI_BASE_HEIGHT));

	UITalkDialogWnd			= new CUITalkDialogWnd();
	UITalkDialogWnd->SetAutoDelete(true);
	AttachChild				(UITalkDialogWnd);
	UITalkDialogWnd->m_pParent = this;
	UITalkDialogWnd->InitTalkDialogWnd();
}

void CUITalkWnd::InitTalkDialog()
{
	m_pActor = Actor();
	if (m_pActor == nullptr || !m_pActor->IsTalking())
	{
		HideDialog();
		return;
	}

	m_pOurInvOwner = m_pActor->cast_inventory_owner();
	m_pOthersInvOwner = m_pActor->GetTalkPartner();

	m_pOurDialogManager = m_pOurInvOwner->cast_phrase_dialog_manager();
	m_pOthersDialogManager = m_pOthersInvOwner->cast_phrase_dialog_manager();

	//имена собеседников
	if (UITalkDialogWnd->UIDialogFrameTop)
		UITalkDialogWnd->UIDialogFrameTop->SetTextIfNodeExist(m_pOthersInvOwner->Name());
	else if (UITalkDialogWnd->UIDialogFrame)
		UITalkDialogWnd->UIDialogFrame->UITitleText.SetTextIfNodeExist(m_pOthersInvOwner->Name());

	if (UITalkDialogWnd->UIDialogFrameBottom)
		UITalkDialogWnd->UIDialogFrameBottom->SetTextIfNodeExist(m_pOurInvOwner->Name());
	else if (UITalkDialogWnd->UIOurPhrasesFrame)
		UITalkDialogWnd->UIOurPhrasesFrame->UITitleText.SetTextIfNodeExist(m_pOurInvOwner->Name());

	UITalkDialogWnd->swapCharacterNames = !CurrentGameUI()->ActorMenu();
	if (UITalkDialogWnd->swapCharacterNames)
	{
		UITalkDialogWnd->UICharacterInfoRight.InitCharacter(m_pOurInvOwner);
		UITalkDialogWnd->UICharacterInfoLeft.InitCharacter(m_pOthersInvOwner);
	}
	else
	{
		UITalkDialogWnd->UICharacterInfoLeft.InitCharacter(m_pOurInvOwner);
		UITalkDialogWnd->UICharacterInfoRight.InitCharacter(m_pOthersInvOwner);
	}

	//очистить лог сообщений
	UITalkDialogWnd->ClearAll();
	UITalkDialogWnd->ResetQuestionSelection();

	UITalkDialogWnd->mechanic_mode = m_pOthersInvOwner->SpecificCharacter().upgrade_mechanic();
	UITalkDialogWnd->SetOsoznanieMode(m_pOthersInvOwner->NeedOsoznanieMode());

	m_hasCameraTarget = false;
	m_waitingForTurn = false;
	m_turnWaitStart = 0;

	if (ShouldWaitForPartnerTurn())
	{
		m_waitingForTurn = true;
		m_turnWaitStart = Device.dwTimeGlobal;
		UITalkDialogWnd->CUIWindow::Show(false);
		UITalkDialogWnd->CUIWindow::Enable(false);
		if (CGameObject* other_go = m_pOthersInvOwner->cast_game_object())
		{
			UpdateCameraDirection(other_go);
		}
		return;
	}

	RevealTalkDialog();
}

void CUITalkWnd::InitOthersStartDialog()
{
	m_pOthersDialogManager->UpdateAvailableDialogs(m_pOurDialogManager);
	if(!m_pOthersDialogManager->AvailableDialogs().empty())
	{
		m_pCurrentDialog = m_pOthersDialogManager->AvailableDialogs().front();
		m_pOthersDialogManager->InitDialog(m_pOurDialogManager, m_pCurrentDialog);
		
		//сказать фразу
		AddAnswer(m_pCurrentDialog->GetPhraseText("0"), m_pOthersInvOwner->NameReal());
		m_pOthersDialogManager->SayPhrase(m_pCurrentDialog, "0");

		//если диалог завершился, перейти в режим выбора темы
		if(!m_pCurrentDialog || m_pCurrentDialog->IsFinished()) ToTopicMode();
	}
}

void CUITalkWnd::NeedUpdateQuestions()
{
	m_bNeedToUpdateQuestions = true;
}

void CUITalkWnd::UpdateQuestions()
{
	UITalkDialogWnd->ClearQuestions();

	//если нет активного диалога, то
	//режима выбора темы
	if(!m_pCurrentDialog)
	{
		m_pOurDialogManager->UpdateAvailableDialogs(m_pOthersDialogManager);
		for(u32 i=0; i< m_pOurDialogManager->AvailableDialogs().size(); ++i)
		{
			const DIALOG_SHARED_PTR& phrase_dialog	= m_pOurDialogManager->AvailableDialogs()[i];
			//if (phrase_dialog->GetPhraseCount() > 0)
			{
				SPhraseInfo phInfo;
				phInfo.sIconName = (phrase_dialog->GetPhrase("0"))->GetIconName();
				phInfo.bUseIconLtx = (phrase_dialog->GetPhrase("0"))->GetIconUsingLTX();
				phInfo.bFinalizer = (phrase_dialog->GetPhrase("0"))->IsFinalizer();

				AddQuestion(phrase_dialog->DialogCaption(), phrase_dialog->GetDialogID(), i, phInfo);
			}
		}
	}
	else
	{
		if(m_pCurrentDialog->IsWeSpeaking(m_pOurDialogManager))
		{
			//если в списке допустимых фраз только одна фраза пустышка, то просто
			//сказать (игрок сам не производит никаких действий)
			if( !m_pCurrentDialog->PhraseList().empty() && m_pCurrentDialog->allIsDummy() ){
				CPhrase* phrase = m_pCurrentDialog->PhraseList()[Random.randI(m_pCurrentDialog->PhraseList().size())];
				SayPhrase(phrase->GetID());
			};

			//выбор доступных фраз из активного диалога
			if( m_pCurrentDialog && !m_pCurrentDialog->allIsDummy() )
			{			
				int number = 0;
				for(PHRASE_VECTOR::const_iterator   it = m_pCurrentDialog->PhraseList().begin();
					it != m_pCurrentDialog->PhraseList().end();
					++it, ++number)
				{
					SPhraseInfo phInfo;
					CPhrase* phrase = *it;
					phInfo.bFinalizer = phrase->IsFinalizer();
					phInfo.sIconName = phrase->GetIconName();
					phInfo.bUseIconLtx = phrase->GetIconUsingLTX();
					AddQuestion(m_pCurrentDialog->GetPhraseText(phrase->GetID() ), phrase->GetID(), number, phInfo);
				}
			}
			else
				UpdateQuestions();
		}
	}
	m_bNeedToUpdateQuestions = false;

}

//////////////////////////////////////////////////////////////////////////

void CUITalkWnd::SendMessage(CUIWindow* pWnd, s16 msg, void* pData)
{
	if(pWnd == UITalkDialogWnd && msg == TALK_DIALOG_TRADE_BUTTON_CLICKED)
	{
		SwitchToTrade();
	}
	else if(pWnd == UITalkDialogWnd && msg == TALK_DIALOG_UPGRADE_BUTTON_CLICKED)
	{
		SwitchToUpgrade();
	}
	else if(pWnd == UITalkDialogWnd && msg == TALK_DIALOG_QUESTION_CLICKED)
	{
		AskQuestion();
	}
	inherited::SendMessage(pWnd, msg, pData);
}

//////////////////////////////////////////////////////////////////////////
int g_TalkCameraStable = 1;
float g_TalkCameraMoveDist = 0.20f;
float g_TalkCameraVertDist = 0.35f;
float g_TalkTurnSpeedHeadDeg = 270.f;
float g_TalkTurnSpeedBodyDeg = 360.f;
float g_TalkBodyTurnAngleDeg = 58.f;
int g_TalkWaitTurn = 1;
float g_TalkWaitStartAngleDeg = 25.f;
float g_TalkWaitDoneAngleDeg = 20.f;
float g_TalkWaitMaxTimeSec = 1.5f;

namespace
{
constexpr u32 kTalkTurnSightGraceMs = 250;

class CCC_TalkInteger : public CCC_Integer
{
public:
	CCC_TalkInteger(const char* N, int* V, int Min, int Max) : CCC_Integer(N, V, Min, Max) {}
	void Save(IWriter*) override {}
};

class CCC_TalkFloat : public CCC_Float
{
public:
	CCC_TalkFloat(const char* N, float* V, float Min, float Max) : CCC_Float(N, V, Min, Max) {}
	void Save(IWriter*) override {}
};

Fvector ComputeTalkCameraTarget(CGameObject* pTo)
{
	Fvector target_pos;
	if (pTo->cast_inventory_owner()->GetFocusingOnNpc())
	{
		if (IKinematics* pk = PKinematics(pTo->Visual()))
		{
			pk->LL_GetBoneWorldPosition(pk->LL_BoneID("bip01_head"), pTo->XFORM(), target_pos);
			return target_pos;
		}
	}
	pTo->Center(target_pos);
	target_pos.y += pTo->Radius() * 0.5f;
	return target_pos;
}

bool ComputeDesiredTalkYaw(CGameObject* npc, CGameObject* actor, float& out_yaw)
{
	Fvector dir;
	dir.sub(actor->Position(), npc->Position());
	dir.y = 0.f;
	if (dir.square_magnitude() <= EPS_S)
		return false;
	float h, p;
	dir.getHP(h, p);
	out_yaw = angle_normalize_signed(-h);
	return true;
}
} // namespace

void CUITalkWnd::UpdateCameraDirection(CGameObject* pTo)
{
	CCameraBase* cam = Actor()->cam_Active();
	if (!cam)
		return;

	const Fvector raw_target = ComputeTalkCameraTarget(pTo);
	const Fvector npc_pos = pTo->Position();

	// Anchor the camera target once the NPC has settled so idle head/torso swaying
	// does not make the camera swim; re-anchor only while turning or on real movement.
	const float move_dist_sq = g_TalkCameraMoveDist * g_TalkCameraMoveDist;
	const bool should_reanchor = !g_TalkCameraStable ||
		!m_hasCameraTarget ||
		m_waitingForTurn ||
		npc_pos.distance_to_sqr(m_cameraNpcPos) > move_dist_sq ||
		std::abs(raw_target.y - m_cameraTargetPos.y) > g_TalkCameraVertDist;
	if (should_reanchor)
	{
		m_cameraTargetPos = raw_target;
		m_cameraNpcPos = npc_pos;
		m_hasCameraTarget = true;
	}

	Fvector target_dir;
	target_dir.sub(m_cameraTargetPos, cam->vPosition);
	if (target_dir.square_magnitude() <= EPS_S)
		return;
	target_dir.normalize();
	float p, h;
	target_dir.getHP(h, p);

	Fvector targ_angles = EulerYawPitchRollInertion({ cam->pitch , cam->yaw, 0.f }, { -p, -h, 0.f }, 0.5f, Device.fTimeDelta);
	cam->pitch = targ_angles.x;
	cam->yaw = targ_angles.y;
}

bool CUITalkWnd::ShouldWaitForPartnerTurn() const
{
	if (!g_TalkWaitTurn || !m_pActor || !m_pOthersInvOwner || m_pOthersInvOwner->NeedOsoznanieMode())
		return false;
	CGameObject* other_go = m_pOthersInvOwner->cast_game_object();
	if (!other_go)
		return false;
	CAI_Stalker* stalker = other_go->cast_stalker();
	if (!stalker || !stalker->g_Alive() || stalker->wounded())
		return false;
	if (stalker->animation_movement_controlled() || stalker->movement().in_smart_cover())
		return false;

	float desired_yaw = 0.f;
	if (!ComputeDesiredTalkYaw(other_go, m_pActor, desired_yaw))
		return false;

	const float head_diff = angle_difference(stalker->movement().m_head.current.yaw, desired_yaw);
	return head_diff > deg2rad(g_TalkWaitStartAngleDeg);
}

bool CUITalkWnd::IsPartnerTurnFinished() const
{
	if (!m_waitingForTurn || !m_pActor || !m_pOthersInvOwner)
		return true;

	const u32 elapsed = Device.dwTimeGlobal - m_turnWaitStart;
	const u32 max_wait_ms = static_cast<u32>(std::max(0.f, g_TalkWaitMaxTimeSec) * 1000.f);
	if (elapsed >= max_wait_ms)
		return true;

	CGameObject* other_go = m_pOthersInvOwner->cast_game_object();
	CAI_Stalker* stalker = other_go ? other_go->cast_stalker() : nullptr;
	if (!stalker || !stalker->g_Alive() || stalker->wounded())
		return true;

	bool looking_at_actor = false;
	if (!stalker->sight().actions().empty())
	{
		const CSightControlAction& act = stalker->sight().current_action();
		looking_at_actor = (act.sight_type() == SightManager::eSightTypeObject) &&
			(act.object_to_look() == m_pActor);
	}
	if (!looking_at_actor)
	{
		return elapsed >= kTalkTurnSightGraceMs;
	}

	float desired_yaw = 0.f;
	if (!ComputeDesiredTalkYaw(other_go, m_pActor, desired_yaw))
		return true;

	const float done_angle = deg2rad(g_TalkWaitDoneAngleDeg);
	if (angle_difference(stalker->movement().m_head.current.yaw, desired_yaw) > done_angle)
		return false;

	if (angle_difference(stalker->movement().m_body.current.yaw, desired_yaw) > deg2rad(g_TalkBodyTurnAngleDeg))
		return false;

	if (stalker->sight().turning_in_place())
	{
		const float body_diff = angle_difference(stalker->movement().m_body.current.yaw, stalker->movement().m_body.target.yaw);
		if (body_diff > done_angle)
			return false;
	}

	return true;
}

void CUITalkWnd::RevealTalkDialog()
{
	m_waitingForTurn = false;

	InitOthersStartDialog();
	if (!IsShown() || !m_pActor)
	{
		return;
	}
	NeedUpdateQuestions();
	Update();
	if (!IsShown() || !m_pActor)
	{
		return;
	}

	UITalkDialogWnd->Show();
	UITalkDialogWnd->UpdateButtonsLayout(b_disable_break, m_pOthersInvOwner->IsTradeEnabled());
}

void CUITalkWnd::Update()
{
	//остановить разговор, если нужно
	if (g_actor && m_pActor && !m_pActor->IsTalking())
	{
		StopTalk();
		return;
	}
	else
	{
		CGameObject* pOurGO = m_pOurInvOwner != nullptr ? m_pOurInvOwner->cast_game_object() : nullptr;
		CGameObject* pOtherGO = m_pOthersInvOwner != nullptr ? m_pOthersInvOwner->cast_game_object() : nullptr;

		if (nullptr == pOurGO || nullptr == pOtherGO)
		{
			HideDialog();
			return;
		}
	}

	if (m_waitingForTurn)
	{
		UpdateCameraDirection(m_pOthersInvOwner->cast_game_object());
		UpdateTalkDof();
		if (IsPartnerTurnFinished())
		{
			RevealTalkDialog();
		}
		return;
	}

	if (m_bNeedToUpdateQuestions)
	{
		UpdateQuestions();
		if (!IsShown() || !m_pActor)
		{
			return;
		}

		if (!UITalkDialogWnd->m_ClickedQuestionID.size() || !UITalkDialogWnd->HasQuestionWithID(UITalkDialogWnd->m_ClickedQuestionID))
			UITalkDialogWnd->SetFirstQuestionSelected();

		UITalkDialogWnd->ScrollSelectionIntoView();
	}
	UITalkDialogWnd->UpdateQuestionSelection();

	inherited::Update();
	UpdateCameraDirection(m_pOthersInvOwner->cast_game_object());
	UpdateTalkDof();
	UITalkDialogWnd->UpdateButtonsLayout(b_disable_break, m_pOthersInvOwner->IsTradeEnabled());

	if (playing_sound())
	{
		CGameObject* pOtherGO = m_pOthersInvOwner != nullptr ? m_pOthersInvOwner->cast_game_object() : nullptr;
		Fvector P = pOtherGO->Position();
		P.y += 1.8f;
		m_sound.set_position(P);
	}
}

void CUITalkWnd::Draw()
{
	inherited::Draw				();
}

void CUITalkWnd::Show(bool status)
{
	if (status && m_pActor && IsShown())
	{
		return;
	}

	inherited::Show(status);
	if (status)
	{
		ActionRepeaters()->ResetAll(nullptr);
		InitTalkDialog();
		if (!IsShown() || !m_pActor)
		{
			return;
		}
		m_pActor->ResetCameraFocus();

		if (m_pOthersInvOwner->GetFocusingOnNpc())
		{
			if (EnableTalkDof && !fsimilar(m_TalkDof.w, -1.0f))
			{
				HasTalkDof = true;
				UpdateTalkDof();
			}

			SavedFov = g_fov;
			HasTalkFov = true;
			g_fov = SavedFov * m_talkFovScale;
		}
	}
	else
	{
		m_waitingForTurn = false;
		m_hasCameraTarget = false;
		RestoreTalkEffects();
		StopSnd();
		if (UITalkDialogWnd->IsShown())
		{
			UITalkDialogWnd->Hide();
		}

		if (m_pActor)
		{
			ToTopicMode();

			if (m_pActor->IsTalking())
			{
				m_pActor->StopTalk();
			}

			m_pActor = nullptr;
		}
	}
}

bool  CUITalkWnd::TopicMode			() 
{
	return nullptr == m_pCurrentDialog.get();
}

void  CUITalkWnd::ToTopicMode		() 
{
	m_pCurrentDialog.reset();// = DIALOG_SHARED_PTR((CPhraseDialog*)nullptr);
}

void CUITalkWnd::UpdateTalkDof()
{
	if (!HasTalkDof || !m_pOthersInvOwner)
	{
		return;
	}

	Fvector TargetPosition;
	m_pOthersInvOwner->cast_game_object()->Center(TargetPosition);
	Fvector TargetDirection;
	TargetDirection.sub(TargetPosition, Device.vCameraPosition);
	const float FocusDistance = std::max(TargetDirection.dotproduct(Device.vCameraDirection), EPS_L);
	// Keep the configured blur distances relative to the conversation partner.
	Fvector Dof;
	Dof.set(FocusDistance + m_TalkDof.x - m_TalkDof.y, FocusDistance, FocusDistance + m_TalkDof.z - m_TalkDof.y);
	GamePersistent().SetEffectorDOF(Dof);
}

void CUITalkWnd::RestoreTalkEffects()
{
	if (HasTalkFov)
	{
		g_fov = SavedFov;
		HasTalkFov = false;
	}
	if (HasTalkDof)
	{
		GamePersistent().RestoreEffectorDOF();
		HasTalkDof = false;
	}
}

void CUITalkWnd::AskQuestion()
{
	if(m_bNeedToUpdateQuestions) return;//quick dblclick:(
	shared_str					phrase_id;

	//игрок выбрал тему разговора
	if(TopicMode())
	{
		if ( (UITalkDialogWnd->m_ClickedQuestionID =="") ||
			(!m_pOurDialogManager->HaveAvailableDialog(UITalkDialogWnd->m_ClickedQuestionID)) ) 
		{

			string128	s;
			xr_sprintf		(s,"ID = [%s] of selected question is out of range of available dialogs ",UITalkDialogWnd->m_ClickedQuestionID.c_str());
			VERIFY2(false, s);
		}

		m_pCurrentDialog = m_pOurDialogManager->GetDialogByID( UITalkDialogWnd->m_ClickedQuestionID);
		
		m_pOurDialogManager->InitDialog(m_pOthersDialogManager, m_pCurrentDialog);
		phrase_id = "0";
	}
	else
	{
		phrase_id = UITalkDialogWnd->m_ClickedQuestionID;
	}

	SayPhrase				(phrase_id);
	NeedUpdateQuestions		();
}

void CUITalkWnd::SayPhrase(const shared_str& phrase_id)
{
	AddAnswer(m_pCurrentDialog->GetPhraseText(phrase_id), m_pOurInvOwner->NameReal());
	m_pOurDialogManager->SayPhrase(m_pCurrentDialog, phrase_id);
	//если диалог завершился, перейти в режим выбора темы
	if(m_pCurrentDialog->IsFinished()) ToTopicMode();
}

void CUITalkWnd::AddQuestion(const shared_str& text, const shared_str& value, int number, SPhraseInfo phInfo)
{
	if(text.size() == 0)
		return;

	UITalkDialogWnd->AddQuestion(text.c_str(), value.c_str(), number, phInfo);
}

void CUITalkWnd::AddAnswer(const shared_str& text, const char* SpeakerName)
{
	//для пустой фразы вообще ничего не выводим
	if(text.size() == 0)
	{
		return;
	}
	PlaySnd			(text.c_str());

	bool i_am = (0 == xr_strcmp(SpeakerName, m_pOurInvOwner->NameReal()));
	UITalkDialogWnd->AddAnswer(SpeakerName, text.c_str(), i_am);
}

void CUITalkWnd::SwitchToTrade()
{
	if ( m_pOurInvOwner->IsTradeEnabled() && m_pOthersInvOwner->IsTradeEnabled() )
	{
		UITalkDialogWnd->Hide();
		CurrentGameUI()->StartTrade	(m_pOurInvOwner, m_pOthersInvOwner);
		StopSnd();
	}
}

void CUITalkWnd::SwitchToUpgrade()
{
	if (CurrentGameUI() && m_pOurInvOwner->IsTradeEnabled() && m_pOthersInvOwner->IsTradeEnabled())
	{
		UITalkDialogWnd->Hide();
		CurrentGameUI()->StartUpgrade(m_pOurInvOwner, m_pOthersInvOwner);
	}
}

bool CUITalkWnd::OnKeyboardAction(int dik, EUIMessages keyboard_action)
{

	if (keyboard_action==WINDOW_KEY_PRESSED)
	{
		if(is_binded(kUSE, dik) || is_binded(kQUIT, dik))
		{
			if(!b_disable_break)
			{
				HideDialog();
				return true;
			}
		}
		else if (!m_waitingForTurn && is_binded(kSPRINT_TOGGLE, dik))
		{
            if (!m_pOthersInvOwner->NeedOsoznanieMode())
            {
                if (UITalkDialogWnd->mechanic_mode)
                    SwitchToUpgrade();
                else
                    SwitchToTrade();
                return true;
            }
		}
	}

	if (m_waitingForTurn)
	{
		return true;
	}

	return inherited::OnKeyboardAction(dik,keyboard_action);
}

bool CUITalkWnd::OnGamepadKeyAction(int id, EUIMessages gamepad_action)
{
	if (gamepad_action==WINDOW_KEY_PRESSED)
	{
		if (m_waitingForTurn)
		{
			if (get_binded_action(id, agUIGeneral) == kUI_BACK && !b_disable_break)
			{
				HideDialog();
			}
			return true;
		}
		switch (get_binded_action(id, agUIGeneral))
		{
			case kUI_BACK:
			{
				if (!b_disable_break)
				{
					HideDialog();
					return true;
				}
				else
				{
					// Exiting without picking a question is disabled
					// So we need to pick a question with finalizer mark (if there is one) and select it
					// In kbd mode this question(btn) has an accelerator key (ESC) tied to it
					return UITalkDialogWnd->TryClickFinalizerQuestion();
				}
			}
			case kUI_ACTION_2:
			{
				if (UITalkDialogWnd->mechanic_mode)
					SwitchToUpgrade();
				else
					SwitchToTrade();
				return true;
			}
			case kUI_UP:
			{
				if (!any_binded_key_for_action_pressed_c(kUI_DOWN))
					UITalkDialogWnd->OffsetQuestionSelection(false, true);
				ActionRepeaters()->SetActionStarted(this, kUI_UP);
				return true;
			}
			case kUI_DOWN:
			{
				if (!any_binded_key_for_action_pressed_c(kUI_UP))
					UITalkDialogWnd->OffsetQuestionSelection(true, true);
				ActionRepeaters()->SetActionStarted(this, kUI_DOWN);
				return true;
			}
			case kUI_SECONDARY_DOWN:
			{
				ActionRepeaters()->SetActionStarted(this, kUI_SECONDARY_DOWN);
				return true;
			}
			case kUI_SECONDARY_UP:
			{
				ActionRepeaters()->SetActionStarted(this, kUI_SECONDARY_UP);
				return true;
			}
			case kUI_ACCEPT:
			{
				VERIFY(UITalkDialogWnd->m_ClickedQuestionID != "");
				if (UITalkDialogWnd->m_ClickedQuestionID != "")
				{
					SendMessage(UITalkDialogWnd, TALK_DIALOG_QUESTION_CLICKED);
				}
				return true;
			}
		}
	}

	return inherited::OnGamepadKeyAction(id,gamepad_action);
}

bool CUITalkWnd::OnGamepadKeyHold(int id)
{
	if (!IR_process()) 
		return false;

	switch (get_binded_action(id, agUIGeneral))
	{
		case kUI_SECONDARY_UP: 
		{
			if (ActionRepeaters()->CanRepeatActionNow(this, kUI_SECONDARY_UP) && !any_binded_key_for_action_pressed_c(kUI_SECONDARY_DOWN))
				UITalkDialogWnd->ScrollLogUp();
			return true;
		}
		case kUI_SECONDARY_DOWN:
		{
			if (ActionRepeaters()->CanRepeatActionNow(this, kUI_SECONDARY_DOWN) && !any_binded_key_for_action_pressed_c(kUI_SECONDARY_UP))
				UITalkDialogWnd->ScrollLogDown();
			return true;
		}
		case kUI_UP:
		{
			if (ActionRepeaters()->CanRepeatActionNow(this, kUI_UP) && !any_binded_key_for_action_pressed_c(kUI_DOWN))
				UITalkDialogWnd->OffsetQuestionSelection(false, false);
			return true;
		}
		case kUI_DOWN:
		{
			if (ActionRepeaters()->CanRepeatActionNow(this, kUI_DOWN) && !any_binded_key_for_action_pressed_c(kUI_UP))
				UITalkDialogWnd->OffsetQuestionSelection(true, false);
			return true;
		}
	}

	return inherited::OnGamepadKeyHold(id);
}

void CUITalkWnd::PlaySnd(const char* text)
{
	u32 text_len = xr_strlen(text);
	
	// Very crude hack with check for maximum path size
	// Script result passes here not the text ID, but the fully localized variant
	if ( text_len == 0 || text_len >= _MAX_PATH)
	{
		return;
	}
	
	string_path	fn;
	
	const char* path = "characters_voice\\dialogs\\";
	const char* ext  = ".ogg";
	u32 tsize   = sizeof(fn) - xr_strlen(path) - xr_strlen(ext) - 1;
	if ( text_len > tsize )
	{
		text_len = tsize;
	}

	strncpy_s( fn, sizeof(fn), path, xr_strlen(path) );
	strncat(fn, text, text_len);
	strncat(fn, ext, xr_strlen(ext));

	//	strconcat( sizeof(fn), fn, "characters_voice\\dialogs\\", text2, ".ogg" );

	StopSnd();
	if (FS.exist(_game_sounds_, fn))
	{
		VERIFY(m_pActor);
		if (!m_pActor->OnDialogSoundHandlerStart(m_pOthersInvOwner, fn))
		{
			CGameObject* pOtherGO = m_pOthersInvOwner->cast_game_object();
			Fvector P = pOtherGO->Position();
			P.y += 1.8f;
			m_sound.create(fn, st_Effect, sg_SourceType);
			m_sound.play_at_pos(0, P);
		}
	}
}

void CUITalkWnd::StopSnd()
{
	if (m_pActor && m_pActor->OnDialogSoundHandlerStop(m_pOthersInvOwner)) return;

	if(m_sound.is_playing()) 
		m_sound.stop	();
}

void CUITalkWnd::AddIconedMessage(const char* caption, const char* text, const char* texture_name, const char* templ_name)
{
	UITalkDialogWnd->AddIconedAnswer(caption, text, texture_name, templ_name);
}

void CUITalkWnd::AddIconedMessage(const char* text, const char* texture_name, Frect texture_rect, const char* templ_name)
{
	UITalkDialogWnd->AddIconedAnswer(text, texture_name, texture_rect, templ_name);
}

void CUITalkWnd::StopTalk()
{
	HideDialog();
}

bool CUITalkWnd::IsActiveTalkUi()
{
	return IsShown();
}

void CUITalkWnd::Stop()
{
}

void RegisterTalkCameraAndTurnConsoleCommands()
{
	CMD4(CCC_TalkInteger, "ai_talk_camera_stable", &g_TalkCameraStable, 0, 1);
	CMD4(CCC_TalkFloat, "ai_talk_camera_move_dist", &g_TalkCameraMoveDist, 0.05f, 1.0f);
	CMD4(CCC_TalkFloat, "ai_talk_camera_vert_dist", &g_TalkCameraVertDist, 0.05f, 1.0f);
	CMD4(CCC_TalkFloat, "ai_talk_turn_speed_head", &g_TalkTurnSpeedHeadDeg, 90.f, 720.f);
	CMD4(CCC_TalkFloat, "ai_talk_turn_speed_body", &g_TalkTurnSpeedBodyDeg, 90.f, 720.f);
	CMD4(CCC_TalkFloat, "ai_talk_body_turn_angle", &g_TalkBodyTurnAngleDeg, 30.f, 120.f);
	CMD4(CCC_TalkInteger, "ai_talk_wait_turn", &g_TalkWaitTurn, 0, 1);
	CMD4(CCC_TalkFloat, "ai_talk_wait_start_angle", &g_TalkWaitStartAngleDeg, 5.f, 90.f);
	CMD4(CCC_TalkFloat, "ai_talk_wait_done_angle", &g_TalkWaitDoneAngleDeg, 5.f, 45.f);
	CMD4(CCC_TalkFloat, "ai_talk_wait_max_time", &g_TalkWaitMaxTimeSec, 0.3f, 4.0f);
}
