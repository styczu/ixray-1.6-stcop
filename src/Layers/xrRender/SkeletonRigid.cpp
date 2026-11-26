//---------------------------------------------------------------------------
#include 	"stdafx.h"


#include 	"SkeletonCustom.h"
#include <cctype>
#include <cmath>
#include <cstring>

extern int	psSkeletonUpdate;

static bool diag_pdog_skelr_match(const char* s)
{
	if (!s || !*s) return false;
	static const char needle[] = "pseudodog";
	for (const char* p = s; *p; ++p)
	{
		size_t i = 0;
		while (needle[i] && p[i] && std::tolower(static_cast<unsigned char>(p[i])) == needle[i])
			++i;
		if (!needle[i]) return true;
	}
	return false;
}

static float diag_pdog_skelr_det3x3(const Fmatrix& m)
{
	return m._11 * (m._22 * m._33 - m._23 * m._32)
		 - m._12 * (m._21 * m._33 - m._23 * m._31)
		 + m._13 * (m._21 * m._32 - m._22 * m._31);
}

static bool diag_pdog_skelr_is_eye_bone(const char* name)
{
	return name && (std::strcmp(name, "bip01_ponytail1") == 0 || std::strcmp(name, "bip01_ponytail2") == 0);
}

static void diag_pdog_skelr_log_bone(CKinematics* K, u16 bid, const char* tag)
{
	CBoneInstance& bi = K->LL_GetBoneInstance(bid);
	const char* bname = K->LL_BoneName_dbg(bid);
	const Fmatrix& T = bi.mTransform;
	const Fmatrix& R = bi.mRenderTransform;
	Msg("[DIAG_PDOG] %s K=%p b=%u(%s) vis=%d cb=%p cbt=%u cbow=%d detT=%.4f detR=%.4f "
		"T.i=(%.3f,%.3f,%.3f) T.j=(%.3f,%.3f,%.3f) T.k=(%.3f,%.3f,%.3f) T.c=(%.4f,%.4f,%.4f) "
		"R.i=(%.3f,%.3f,%.3f) R.j=(%.3f,%.3f,%.3f) R.k=(%.3f,%.3f,%.3f) R.c=(%.4f,%.4f,%.4f)",
		tag, K, unsigned(bid), bname ? bname : "?", int(K->LL_GetBoneVisible(bid)),
		reinterpret_cast<void*>(bi.callback()), unsigned(bi.callback_type()), int(bi.callback_overwrite()),
		diag_pdog_skelr_det3x3(T), diag_pdog_skelr_det3x3(R),
		T.i.x, T.i.y, T.i.z, T.j.x, T.j.y, T.j.z, T.k.x, T.k.y, T.k.z, T.c.x, T.c.y, T.c.z,
		R.i.x, R.i.y, R.i.z, R.j.x, R.j.y, R.j.z, R.k.x, R.k.y, R.k.z, R.c.x, R.c.y, R.c.z);
}

static void diag_pdog_skelr_inspect(CKinematics* K, const char* vis_name, u32 now_ms)
{
	const u16 bcount = K->LL_BoneCount();
	if (bcount == 0) return;

	const u16 head_id = K->LL_BoneID("bip01_head");
	const u16 jaw_id  = K->LL_BoneID("bone01");
	const u16 jaw2_id = K->LL_BoneID("bone02");

	u16 bad_bone = BI_NONE;
	const char* reason = nullptr;
	bool is_ragdoll = false;

	if (head_id != BI_NONE && head_id < bcount)
		is_ragdoll = (K->LL_GetBoneInstance(head_id).callback_type() == bctPhysics);
	if (!is_ragdoll && jaw_id != BI_NONE && jaw_id < bcount)
		is_ragdoll = (K->LL_GetBoneInstance(jaw_id).callback_type() == bctPhysics);

	for (u16 b = 0; b < bcount; ++b)
	{
		const char* bn = K->LL_BoneName_dbg(b);
		if (diag_pdog_skelr_is_eye_bone(bn))
			continue;
		const CBoneInstance& bi = K->LL_GetBoneInstance(b);
		if (!K->LL_GetBoneVisible(b))
		{
			bad_bone = b; reason = "invisible"; break;
		}
		if (!_valid(bi.mTransform) || !_valid(bi.mRenderTransform))
		{
			bad_bone = b; reason = "invalid_nan"; break;
		}
		if (std::fabs(diag_pdog_skelr_det3x3(bi.mTransform) - 1.f) > 0.05f ||
			std::fabs(diag_pdog_skelr_det3x3(bi.mRenderTransform) - 1.f) > 0.05f)
		{
			bad_bone = b; reason = "bad_det"; break;
		}
	}

	if (bad_bone == BI_NONE && head_id != BI_NONE && jaw_id != BI_NONE && head_id < bcount && jaw_id < bcount)
	{
		const float d = K->LL_GetBoneInstance(jaw_id).mTransform.c.distance_to(
			K->LL_GetBoneInstance(head_id).mTransform.c);
		if (d > 0.25f)
		{
			bad_bone = jaw_id;
			reason = "jaw_far_from_head";
		}
	}

	if (!reason && !is_ragdoll)
		return;

	struct DiagSlot
	{
		const void* ptr = nullptr;
		u32 last_anom_ms = 0;
		u32 anom_count = 0;
		bool logged_ragdoll = false;
	};
	static xrCriticalSection s_cs;
	static DiagSlot s_slots[64];
	static u32 s_next = 0;

	bool do_log = false;
	const char* kind = "RAGDOLL_FIRST";
	{
		xrCriticalSectionGuard g(&s_cs);
		DiagSlot* slot = nullptr;
		for (auto& s : s_slots)
		{
			if (s.ptr == K) { slot = &s; break; }
		}
		if (!slot)
		{
			slot = &s_slots[s_next++ & 63];
			*slot = DiagSlot{};
			slot->ptr = K;
		}
		if (reason)
		{
			if (slot->anom_count < 4 && (slot->anom_count == 0 || now_ms - slot->last_anom_ms >= 500))
			{
				slot->anom_count++;
				slot->last_anom_ms = now_ms;
				do_log = true;
				kind = reason;
			}
		}
		else if (is_ragdoll && !slot->logged_ragdoll)
		{
			slot->logged_ragdoll = true;
			do_log = true;
			kind = "RAGDOLL_FIRST";
		}
	}

	if (!do_log)
		return;

	const float dist_hj = (head_id != BI_NONE && jaw_id != BI_NONE && head_id < bcount && jaw_id < bcount)
		? K->LL_GetBoneInstance(jaw_id).mTransform.c.distance_to(K->LL_GetBoneInstance(head_id).mTransform.c)
		: -1.f;
	Msg("[DIAG_PDOG] CalcBones(%s) K=%p vis='%s' bcount=%u root=%u mask=0x%016llX bad=%u dist_hj=%.4f",
		kind, K, vis_name ? vis_name : "?", unsigned(bcount), unsigned(K->LL_GetBoneRoot()),
		static_cast<unsigned long long>(K->LL_GetBonesVisible()._visimask.flags),
		unsigned(bad_bone), dist_hj);
	if (head_id != BI_NONE && head_id < bcount) diag_pdog_skelr_log_bone(K, head_id, "CalcBones");
	if (jaw_id  != BI_NONE && jaw_id  < bcount) diag_pdog_skelr_log_bone(K, jaw_id,  "CalcBones");
	if (jaw2_id != BI_NONE && jaw2_id < bcount) diag_pdog_skelr_log_bone(K, jaw2_id, "CalcBones");
	if (bad_bone != BI_NONE && bad_bone < bcount && bad_bone != head_id && bad_bone != jaw_id && bad_bone != jaw2_id)
		diag_pdog_skelr_log_bone(K, bad_bone, "CalcBones");
}

#ifdef DEBUG
void check_kinematics(CKinematics* _k, const char* s);
#endif

void CKinematics::CalculateBones(bool bForceExact)
{
	PROF_EVENT("CKinematics::CalculateBones");
	// early out.
	// check if the info is still relevant
	// skip all the computations - assume nothing changes in a small period of time :)
	if (RDEVICE.dwTimeGlobal == UCalc_Time)
		return;	// early out for "fast" update

	xrCriticalSectionGuard guard(&UCalc_Mutex);
	OnCalculateBones();

	if (!bForceExact && (RDEVICE.dwTimeGlobal < (UCalc_Time + UCalc_Interval)))	
		return;	// early out for "slow" update

	if (Update_Visibility)
		Visibility_Update();


	// here we have either:
	//	1:	timeout elapsed
	//	2:	exact computation required
	UCalc_Time = RDEVICE.dwTimeGlobal;

	// exact computation
	// Calculate bones
#ifdef DEBUG
	RDEVICE.Statistic->Animation.Begin();
#endif

	Bone_Calculate(bones->at(iRoot),&Fidentity);
	if (diag_pdog_skelr_match(dbg_name.c_str()))
		diag_pdog_skelr_inspect(this, dbg_name.c_str(), RDEVICE.dwTimeGlobal);
#ifdef DEBUG
	check_kinematics				(this, dbg_name.c_str() );
	RDEVICE.Statistic->Animation.End	();
#endif
	VERIFY( LL_GetBonesVisible()._visimask.flags !=0 );
	// Calculate BOXes/Spheres if needed
	UCalc_Visibox++; 
	if (UCalc_Visibox>=psSkeletonUpdate) 
	{
		// mark
		UCalc_Visibox = -(::Random.randI(psSkeletonUpdate-1));

		CalculateBBox();

#ifdef DEBUG
		// Validate
		VERIFY3	(_valid(vis.box.min)&&_valid(vis.box.max),	"Invalid bones-xform in model", dbg_name.c_str());
		if(vis.sphere.R>1000.f)
		{
			for(u16 ii=0; ii<LL_BoneCount();++ii){
				Fmatrix tr;
				tr = LL_GetTransform(ii);
				Msg("bone %s",LL_BoneName_dbg(ii));
				Log("bone_matrix",tr);
			}
			Log("end-------");
		}
		VERIFY3	(vis.sphere.R<1000.f,						"Invalid bones-xform in model", dbg_name.c_str());
#endif
	}

	//
	if (Update_Callback)
		Update_Callback(this);
}

#ifdef DEBUG
void check_kinematics(CKinematics* _k, const char* s)
{
	CKinematics* K = _k;
	Fmatrix&	MrootBone		= K->LL_GetBoneInstance(K->LL_GetBoneRoot()).mTransform;
	if(MrootBone.c.y >10000)
	{	
		Msg("all bones transform:--------[%s]",s);
		
		for(u16 ii=0; ii<K->LL_BoneCount();++ii){
			Fmatrix tr;

			tr = K->LL_GetTransform(ii);
			Msg("bone %s",K->LL_BoneName_dbg(ii));
			Log("bone_matrix",tr);
		}
		Log("end-------");
		VERIFY3(0,"check_kinematics failed for ", s);
	}
}
#endif

void CKinematics::BuildBoneMatrix( const CBoneData* bd, CBoneInstance &bi, const Fmatrix *parent, u8 channel_mask/* = (1<<0)*/ )
{
	bi.mTransform.mul_43(*parent,bd->bind_transform);
}

void CKinematics::CLBone( const CBoneData* bd, CBoneInstance &bi, const Fmatrix *parent, u8 channel_mask /*= (1<<0)*/)
{
	u16 SelfID = bd->GetSelfID();

	if (LL_GetBoneVisible(SelfID))
	{
		if (bi.callback_overwrite())
		{
			if (bi.callback())
				bi.callback()(&bi);
		}
		else
		{
			BuildBoneMatrix( bd, bi, parent, channel_mask );
			if (!_valid(bi.mTransform) && diag_pdog_skelr_match(dbg_name.c_str()))
				Msg("[DIAG_PDOG] CLBone INVALID after BuildBoneMatrix K=%p vis='%s' bone=%u(%s)", this, dbg_name.c_str(), unsigned(SelfID), bd->name.c_str());
#ifndef MASTER_GOLD
			R_ASSERT2( _valid( bi.mTransform ), "anim kils bone matrix" ); 
#endif // #ifndef MASTER_GOLD
			if (bi.callback())
			{
				bi.callback()(&bi);
				if (!_valid(bi.mTransform) && diag_pdog_skelr_match(dbg_name.c_str()))
					Msg("[DIAG_PDOG] CLBone INVALID after callback K=%p vis='%s' bone=%u(%s) cb=%p cbt=%u", this, dbg_name.c_str(), unsigned(SelfID), bd->name.c_str(), reinterpret_cast<void*>(bi.callback()), unsigned(bi.callback_type()));
#ifndef MASTER_GOLD
				R_ASSERT2( _valid( bi.mTransform ), make_string<const char*>( "callback kils bone matrix bone: %s " , bd->name.c_str() ) );
#endif // #ifndef MASTER_GOLD
			}
		}
		bi.mRenderTransform.mul_43(bi.mTransform,bd->m2b_transform);
	}
}

void CKinematics::Bone_GetAnimPos(Fmatrix& pos,u16 id,u8 mask_channel, bool ignore_callbacks)
{
	R_ASSERT(id<LL_BoneCount());
	CBoneInstance bi = bone_instances[id];
	Fvector last_c = bi.mTransform.c;
	BoneChain_Calculate(&LL_GetData(id),bi,mask_channel,ignore_callbacks);
#ifndef MASTER_GOLD
	R_ASSERT( _valid( bi.mTransform ) );
#endif
	pos.set( bi.mTransform );
	pos.c.set(last_c);
}

void CKinematics::Bone_Calculate(CBoneData* bd, Fmatrix *parent)
{
	xrCriticalSectionGuard guard(&UCalc_Mutex2);
	u16 SelfID = bd->GetSelfID();
	CBoneInstance &BONE_INST = bone_instances[SelfID];
	CLBone( bd, BONE_INST, parent, u8(-1) );
	// Calculate children
	for (CBoneData* BD : bd->children)
		Bone_Calculate(BD, &BONE_INST.mTransform);

}

void CKinematics::BoneChain_Calculate(const CBoneData* bd, CBoneInstance &bi, u8 mask_channel, bool ignore_callbacks)
{
	u16 SelfID = bd->GetSelfID();

	//ignore callbacks
	BoneCallback bc = bi.callback();
	bool ow = bi.callback_overwrite();
	if(ignore_callbacks)
		bi.set_callback( bi.callback_type(), nullptr, bi.callback_param(), false );

	if(SelfID==LL_GetBoneRoot())
	{
		CLBone( bd, bi, &Fidentity, mask_channel );
		//restore callback	
		bi.set_callback( bi.callback_type(), bc, bi.callback_param(), ow );
		return;
	}
	u16 ParentID = bd->GetParentID();
	R_ASSERT( ParentID != BI_NONE );
	CBoneData* ParrentDT = &LL_GetData(ParentID);
	CBoneInstance parrent_bi = bone_instances[ParentID];
	BoneChain_Calculate(ParrentDT, parrent_bi, mask_channel, ignore_callbacks);
	CLBone( bd, bi, &parrent_bi.mTransform, mask_channel );
	//restore callback
	bi.set_callback( bi.callback_type(), bc, bi.callback_param(), ow );

}
