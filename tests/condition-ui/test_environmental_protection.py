"""Compile production protection readers, damage pipeline and tooltip builder.

Uses fixtures for engine services, item wear and widgets, not for protection maths.
Run with --mod-root; optional --examples writes the actual formatted PL tooltips.
"""
from pathlib import Path
import argparse
import json
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser()
parser.add_argument('--mod-root', type=Path, required=True)
parser.add_argument('--examples', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
def read(path):
    return (root/path).read_bytes().decode('latin1').replace('\r\n', '\n')
def function(path, signature):
    text=read('src/xrGame/'+path);start=text.index(signature);end=text.index('{',start)+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]+'\n'
def strings(path, encoding):
    text=path.read_bytes().decode(encoding)
    tree=ET.fromstring(text[text.index('?>')+2:])
    pairs=[(e.get('id'),e.findtext('text','')) for e in tree]
    assert len({k for k,v in pairs})==len(pairs),path
    return dict(pairs)
def literal(text):
    return '"'+''.join('\\%03o'%byte for byte in text.encode('cp1250'))+'"'

cpp=r'''
#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using s16=short;using u32=uint32_t;using LPCSTR=const char*;using xr_string=std::string;using std::pow;
using string32=char[32];using string64=char[64];
#define VERIFY(x) assert(x)
#define flt_max FLT_MAX
#define R_ASSERT2(x,...) assert(x)
template<class T>void clamp(T&v,T lo,T hi){v=std::max(lo,std::min(v,hi));}
size_t xr_strlen(const char*s){return std::strlen(s);}int xr_strcmp(const char*a,const char*b){return std::strcmp(a,b);}
template<size_t N,class...Ts>void xr_strconcat(char(&out)[N],Ts...parts){std::string s;(s.append(parts),...);assert(s.size()<N);std::strcpy(out,s.c_str());}
struct shared_str{std::string s;shared_str(const char*v=""):s(v){}shared_str(std::string v):s(v){}const char*c_str()const{return s.c_str();}size_t size()const{return s.size();}};
struct Strings{std::map<std::string,std::string> values;shared_str translate(const char*k){auto it=values.find(k);return it==values.end()?shared_str(k):shared_str(it->second);}} table;
Strings*g_pStringTable=&table;
'''
cpp+='namespace ALife {\n'+re.search(r'enum EHitType\s*\{.*?\};',read('src/xrEngine/AI/alife_space.h'),re.S)[0]+'\n}\n#define XRAY_ALIFE_SPACE\n'
cpp+='#include "ProtectionValues.h"\n#include "EnvironmentalExposure.h"\n#include "EnvironmentalDamage.h"\n#include "ConditionUiValues.h"\n'
cpp+='namespace ConditionUi { char DecimalSeparator(){return \',\';}\n'+function('ui/UIConditionFormat.h','    inline void FormatProtectionPoints(')+'}\n'
cpp+=r'''
namespace ALife {const char*g_cafHitType2String(EHitType){return "hit";}}
struct {u32 dwTimeGlobal=0;}Device;
struct CArtefact;
struct Item {float condition=1;std::string name="artifact";float GetCondition(){return condition;}virtual CArtefact*cast_artefact(){return nullptr;}const char*NameItem(){return name.c_str();}};
struct Immunities {float v[ALife::eHitTypeMax]={};float AffectHit(float p,ALife::EHitType t){return p*v[t];}};
struct CArtefact:Item{Immunities m_ArtefactHitImmunities;CArtefact*cast_artefact()override{return this;}};
using PIItem=Item*;struct Inventory{std::vector<PIItem>m_belt;};
struct Bones{float m_fHitFracActor=.1f;float getBoneProtection(s16){return 1;}}bones;
bool IsGameTypeSingle(){return true;}
struct CCustomOutfit:Item{float m_HitTypeProtection[ALife::eHitTypeMax]={};Bones*m_boneProtection=&bones;bool bIsHelmetAvaliable=true;
 float GetDefHitTypeProtection(ALife::EHitType);float HitThroughArmor(float,s16,float,bool&,ALife::EHitType);
 float GetBoneArmor(s16){return .2f;}void Hit(float,ALife::EHitType){} };
struct CHelmet:CCustomOutfit{float GetDefHitTypeProtection(ALife::EHitType);float HitThroughArmor(float,s16,float,bool&,ALife::EHitType);};
struct CInventoryOwner{CCustomOutfit*outfit=nullptr;CHelmet*helmet=nullptr;CCustomOutfit*GetOutfit(){return outfit;}CHelmet*GetHelmet(){return helmet;}};
struct Object{CInventoryOwner*owner=nullptr;CInventoryOwner*cast_inventory_owner(){return owner;}int ID(){return 1;}const char*Name(){return "actor";}void*Visual(){return nullptr;}};
struct Kinematics{const char*LL_BoneName_dbg(int){return "bone";}}kinematics;
Kinematics*PKinematics(void*){return &kinematics;}void Msg(const char*,...){}
constexpr int BI_NONE=-1;bool bDebug=false;
struct CWound{}wound;
struct SHit{Object*who=nullptr;int boneID=0;bool add_wound=false;float power=0,armor_piercing=0;ALife::EHitType hit_type=ALife::eHitTypeBurn;float damage(){return power;}};
struct CEntityCondition{
 Object*m_object=nullptr,*m_pWho=nullptr;int m_iWhoID=0;
 float immunity[ALife::eHitTypeMax]={};
 float m_fBoostTelepaticProtection=0,m_fBoostTelepaticImmunity=0,m_fBoostBurnImmunity=0;
 float m_fBoostChemicalBurnProtection=0,m_fBoostChemicalBurnImmunity=0,m_fBoostShockImmunity=0;
 float m_fBoostRadiationProtection=0,m_fBoostRadiationImmunity=0,m_fBoostExplImmunity=0;
 float m_fBoostStrikeImmunity=0,m_fBoostFireWoundImmunity=0,m_fBoostWoundImmunity=0;
 float m_fHealthLost=0,m_fHealthHitPart=1,m_fPowerHitPart=.2f,m_fHitBoneScale=1,m_fWoundBoneScale=1;
 float m_fDeltaHealth=0,m_fDeltaPsyHealth=0,m_fDeltaRadiation=0,m_fDeltaPower=0;
 float GetHitImmunity(ALife::EHitType t)const{return immunity[t];}
 float HitOutfitEffect(float,ALife::EHitType,s16,float,bool&);
 void ChangePsyHealth(float amount){m_fDeltaPsyHealth+=amount;}bool CanBeHarmed(){return true;}float GetHealth(){return 1;}
 CWound*AddWound(float,ALife::EHitType,int){return &wound;}CWound*ConditionHit(SHit*);
 float GetEnvironmentalProtectionBoost(ALife::EHitType)const;float GetEnvironmentalHitMultiplier(ALife::EHitType)const;
};
struct CActorCondition:CEntityCondition{
 Protection::DamageHistory m_environmental_damage;
 Protection::DamageRate GetEnvironmentalDamageRate(ALife::EHitType)const;Protection::DamageReading GetEnvironmentalDamage(ALife::EHitType)const;
 float radiation=.123f;float GetRadiation(){return radiation;}
 float GetZoneMaxPower(ALife::EHitType t){return t==ALife::eHitTypeShock?.8f:t==ALife::eHitTypeRadiation?.03f:t==ALife::eHitTypeTelepatic?.1f:.2f;}
};
struct CActor:CInventoryOwner{
 Object object;CActorCondition cond;Inventory inv;Protection::ExposureHistory m_environmental_exposure;
 CActor(){object.owner=this;cond.m_object=&object;for(auto&v:cond.immunity)v=.7f;}
 Inventory&inventory(){return inv;}CActorCondition&conditions(){return cond;}
 float HitArtefactsOnBelt(float,ALife::EHitType);float GetProtection_ArtefactsOnBelt(ALife::EHitType);float GetEquipmentProtection(ALife::EHitType);
 Protection::ExposureReading GetEnvironmentalExposure(ALife::EHitType)const;
};
enum EStateType{stt_fire,stt_shock,stt_acid,stt_radia,stt_psi,stt_count};
struct State{std::string hint;float peak=0,protection=0,opacity=0;
 void set_hint_text(const char*s){hint=s;}void set_environmental_exposure(ALife::EHitType,float p,float t,float a){peak=p;protection=t;opacity=a;}};
struct ui_actor_state_wnd{State*m_state[stt_count]={};void UpdateProtectionHints(CActor*);};
const char*kColTitle="%c[255,224,230,234]",*kColSep="%c[255,95,104,110]",*kColLabel="%c[255,176,182,186]",*kBreak="\\n";
'''
for file, signature in [
 ('Actor.cpp','float CActor::HitArtefactsOnBelt'),('Actor.cpp','float CActor::GetProtection_ArtefactsOnBelt'),
 ('Actor.cpp','float CActor::GetEquipmentProtection'),('Actor.cpp','Protection::ExposureReading CActor::GetEnvironmentalExposure'),
 ('CustomOutfit.cpp','float CCustomOutfit::GetDefHitTypeProtection'),('CustomOutfit.cpp','float CCustomOutfit::HitThroughArmor'),
 ('ActorHelmet.cpp','float CHelmet::GetDefHitTypeProtection'),('ActorHelmet.cpp','float CHelmet::HitThroughArmor'),
 ('EntityCondition.cpp','float CEntityCondition::HitOutfitEffect'),('EntityCondition.cpp','CWound* CEntityCondition::ConditionHit'),
 ('EntityCondition.cpp','float CEntityCondition::GetEnvironmentalProtectionBoost'),('EntityCondition.cpp','float CEntityCondition::GetEnvironmentalHitMultiplier'),
 ('ActorCondition.cpp','Protection::DamageRate CActorCondition::GetEnvironmentalDamageRate'),
 ('ActorCondition.cpp','Protection::DamageReading CActorCondition::GetEnvironmentalDamage'),
 ('ui/UIActorStateInfo.cpp','void ui_actor_state_wnd::UpdateProtectionHints'),
]:cpp+=function(file,signature)
cpp+='CActor* expressionActor=nullptr;CActor* GetActor(){return expressionActor;}\n'
delegates=read('src/xrGame/game_expression_delegates.cpp')
cpp+=delegates[delegates.index('#define DECLARE_EQUIPMENT_PROTECTION_RATIO'):delegates.index('#undef DECLARE_EQUIPMENT_PROTECTION_RATIO')]+'\n'
cpp+=r'''
int checks=0;
void eq(double got,double want){++checks;if(std::abs(got-want)>1e-6+std::abs(want)*1e-5){std::cerr<<got<<" != "<<want<<"\n";std::abort();}}
std::string plain(std::string s){for(size_t i;(i=s.find("%c["))!=std::string::npos;)s.erase(i,s.find(']',i)-i+1);return s;}
std::string row(const State&s,const char*key){std::string t=plain(s.hint),label=table.translate(key).s+":";auto a=t.find(label);assert(a!=std::string::npos);a+=label.size();while(t[a]==' ')++a;auto b=t.find("\\n",a);return t.substr(a,b==std::string::npos?b:b-a);}
void has(const State&s,const char*key,bool yes){++checks;if((s.hint.find(table.translate(key).s)!=std::string::npos)!=yes){std::cerr<<"visibility "<<key<<" expected "<<yes<<"\n"<<plain(s.hint)<<"\n";std::abort();}}
void textEq(std::string a,std::string b){++checks;if(a!=b){std::cerr<<a<<" != "<<b<<"\n";std::abort();}}
float resolved(CActor&a,ALife::EHitType type,float raw){auto&c=a.cond;c.m_fDeltaHealth=c.m_fDeltaPsyHealth=c.m_fDeltaRadiation=0;
 SHit hit;hit.hit_type=type;hit.power=a.HitArtefactsOnBelt(raw,type);c.ConditionHit(&hit);
 return type==ALife::eHitTypeRadiation?c.m_fDeltaRadiation:type==ALife::eHitTypeTelepatic?-c.m_fDeltaPsyHealth:-c.m_fDeltaHealth;}
void sample(const char*name,const State&s){std::cout<<"EXAMPLE "<<name<<"\n"<<plain(s.hint)<<"\nEND EXAMPLE\n";}
int main(){
'''
pl=strings(args.mod_root/'configs/text/pol/zz_uiparams_panel.xml','cp1250')
for key,value in pl.items():
    if key.startswith('ui_uip_protection_') or key in ['ui_uip_unit_protection','ui_uip_unit_rad','ui_uip_unit_percent_s'] or key.startswith('ui_inv_outfit_'):
        cpp+='table.values['+literal(key)+']='+literal(value)+';\n'
cpp+=r'''
 CActor actor;expressionActor=&actor;CCustomOutfit outfit;CHelmet helmet;CArtefact art,second,irrelevant,spent;Item unrelated;
 actor.outfit=&outfit;actor.helmet=&helmet;ui_actor_state_wnd panel;State states[stt_count];
 for(int i=0;i<stt_count;++i)panel.m_state[i]=&states[i];
 const auto burn=ALife::eHitTypeBurn,light=ALife::eHitTypeLightBurn,chem=ALife::eHitTypeChemicalBurn,rad=ALife::eHitTypeRadiation,psi=ALife::eHitTypeTelepatic,shock=ALife::eHitTypeShock;
 const auto types={burn,light,shock,chem,rad,psi};
 // A-E: actual artefact curve is inverted only to create a 25% fixture.
 const float coefficient=4*std::log(.9f)/std::log(.25f/1.5f);
 for(auto t:types){outfit.m_HitTypeProtection[t]=1.5f;helmet.m_HitTypeProtection[t]=.5f;art.m_ArtefactHitImmunities.v[t]=coefficient;}
 actor.inv.m_belt={&art};
 eq(actor.HitArtefactsOnBelt(1,burn),.75f);
 auto threshold=Protection::EffectiveThreshold(.15f,.05f,0,.75f);eq(threshold.power,.266666667);assert(threshold.attainable);
 eq(resolved(actor,burn,threshold.power),0);eq(resolved(actor,burn,.4f),.07f);
 actor.cond.m_fBoostChemicalBurnProtection=.05f;
 threshold=Protection::EffectiveThreshold(.15f,.05f,actor.cond.GetEnvironmentalProtectionBoost(chem),actor.HitArtefactsOnBelt(1,chem));
 eq(threshold.power,.333333333);eq(resolved(actor,chem,threshold.power),0);
 actor.inv.m_belt.clear();eq(Protection::EffectiveThreshold(.15f,.05f,0,actor.HitArtefactsOnBelt(1,burn)).power,.2f);
 outfit.m_HitTypeProtection[burn]=.5f;outfit.condition=.5f;eq(outfit.GetDefHitTypeProtection(burn)*.1f,.025f);
 outfit.condition=1;outfit.m_HitTypeProtection[burn]=1.5f;
 // All actual type-specific boosters, per-type actor immunity and threshold boundaries.
 for(auto t:types)for(float difficulty:{.3f,.7f,.85f,1.f})for(float boost:{0.f,.1f,1.2f}){
  actor.cond.immunity[t]=difficulty;actor.cond.immunity[burn]=difficulty;
  actor.cond.m_fBoostBurnImmunity=actor.cond.m_fBoostShockImmunity=boost;
  actor.cond.m_fBoostChemicalBurnImmunity=actor.cond.m_fBoostRadiationImmunity=actor.cond.m_fBoostTelepaticImmunity=boost;
  actor.cond.m_fBoostChemicalBurnProtection=.05f;actor.cond.m_fBoostRadiationProtection=.03f;actor.cond.m_fBoostTelepaticProtection=.02f;
  for(float resistance:{-.1f,0.f,.04f,.1f,coefficient,.99f,2.f}){
   actor.inv.m_belt={&art};art.m_ArtefactHitImmunities.v[t]=resistance;
   const float A=actor.HitArtefactsOnBelt(1,t),B=actor.cond.GetEnvironmentalProtectionBoost(t),D=actor.cond.GetEnvironmentalHitMultiplier(t);
   eq(D,difficulty-boost);
   eq(B,t==chem?.05f:t==rad?.03f:t==psi?.02f:0);
   auto e=Protection::EffectiveThreshold(.15f,.05f,B,A);assert(e.attainable);eq(e.power,(.2f+B)/A);eq(actor.GetEquipmentProtection(t),e.power);
   eq(resolved(actor,t,e.power*.5f),0);eq(resolved(actor,t,e.power),0);
   eq(resolved(actor,t,e.power+.1f),.1f*A*D);
  }
 }
 // F: sum coefficients before the nonlinear function; only belt and live conditions count.
 for(auto t:types){actor.cond.immunity[t]=.7f;art.m_ArtefactHitImmunities.v[t]=0;second.m_ArtefactHitImmunities.v[t]=0;}
 actor.cond.m_fBoostBurnImmunity=actor.cond.m_fBoostShockImmunity=actor.cond.m_fBoostChemicalBurnImmunity=actor.cond.m_fBoostRadiationImmunity=actor.cond.m_fBoostTelepaticImmunity=0;
 actor.cond.m_fBoostChemicalBurnProtection=actor.cond.m_fBoostRadiationProtection=actor.cond.m_fBoostTelepaticProtection=0;
 art.m_ArtefactHitImmunities.v[burn]=.1f;second.m_ArtefactHitImmunities.v[burn]=.1f;
 actor.inv.m_belt={&art,&second,&unrelated};eq(actor.HitArtefactsOnBelt(1,burn),.817635018);
 second.condition=0;eq(actor.HitArtefactsOnBelt(1,burn),.977828695);second.condition=1;
 // Live formatting, filtering, titles, temporary effects, penalties and unusual signed D.
 art.name="Artefakt testowy";second.name="SECOND";irrelevant.name="IRRELEVANT";spent.name="SPENT";spent.condition=0;
 spent.m_ArtefactHitImmunities.v[burn]=1;irrelevant.m_ArtefactHitImmunities.v[chem]=1;
 actor.inv.m_belt.clear();panel.UpdateProtectionHints(&actor);
 textEq(row(states[stt_fire],"ui_uip_protection_effective"),"1000,00 pkt");
 textEq(row(states[stt_fire],"ui_uip_protection_total"),"1000,00 pkt");
 has(states[stt_fire],"ui_uip_protection_received",false);has(states[stt_fire],"ui_uip_protection_after_threshold",false);
 has(states[stt_fire],"ui_uip_protection_artifact_effect",false);has(states[stt_fire],"ui_uip_protection_temporary",false);has(states[stt_fire],"ui_uip_protection_active",false);
 sample("burn-no-artifacts",states[stt_fire]);
 art.m_ArtefactHitImmunities.v[burn]=art.m_ArtefactHitImmunities.v[light]=coefficient;actor.inv.m_belt={&art,&irrelevant,&spent,&unrelated};
 panel.UpdateProtectionHints(&actor);
 textEq(row(states[stt_fire],"ui_uip_protection_effective"),"1333,33 pkt");
 textEq(row(states[stt_fire],"ui_uip_protection_artifact_effect"),"25,00%");
 textEq(row(states[stt_fire],"ui_uip_protection_total"),"1000,00 pkt");
 assert(states[stt_fire].hint.find("Artefakt testowy")!=std::string::npos&&states[stt_fire].hint.find("IRRELEVANT")==std::string::npos&&states[stt_fire].hint.find("SPENT")==std::string::npos);
 actor.m_environmental_exposure.Record(burn,.65f,500);Device.dwTimeGlobal=700;actor.m_environmental_exposure.Record(burn,.4f,700);
 actor.cond.m_environmental_damage.Record(burn,0,0,0,200);actor.cond.m_environmental_damage.Record(burn,.035f,0,0,700);panel.UpdateProtectionHints(&actor);
 textEq(row(states[stt_fire],"ui_uip_protection_source"),"3250,00 pkt");textEq(row(states[stt_fire],"ui_uip_protection_damage"),"3,50%");
 has(states[stt_fire],"ui_uip_protection_current",false);has(states[stt_fire],"ui_uip_protection_peak",false);has(states[stt_fire],"ui_uip_protection_active",false);
 sample("burn-artifact",states[stt_fire]);
 art.m_ArtefactHitImmunities.v[chem]=coefficient;actor.inv.m_belt={&art};actor.cond.m_fBoostChemicalBurnProtection=.05f;
 actor.m_environmental_exposure.Record(chem,.4f,700);actor.cond.m_environmental_damage.Record(chem,0,0,0,200);actor.cond.m_environmental_damage.Record(chem,.0175f,0,0,700);panel.UpdateProtectionHints(&actor);
 textEq(row(states[stt_acid],"ui_uip_protection_effective"),"1666,67 pkt");textEq(row(states[stt_acid],"ui_uip_protection_temporary"),"250,00 pkt");
 sample("chemical-booster",states[stt_acid]);
 // Difficulty/immune boosters alter D only. No clamping, even when D <= 0.
 actor.cond.m_fBoostBurnImmunity=.2f;panel.UpdateProtectionHints(&actor);has(states[stt_fire],"ui_uip_protection_received",false);textEq(row(states[stt_fire],"ui_uip_protection_effective"),"1333,33 pkt");
 actor.cond.m_fBoostBurnImmunity=1;panel.UpdateProtectionHints(&actor);has(states[stt_fire],"ui_uip_protection_received",false);
 actor.cond.m_fBoostBurnImmunity=.7f;panel.UpdateProtectionHints(&actor);has(states[stt_fire],"ui_uip_protection_received",false);
 art.m_ArtefactHitImmunities.v[burn]=-.1f;panel.UpdateProtectionHints(&actor);textEq(row(states[stt_fire],"ui_uip_protection_artifact_effect"),"-2,22%");
 // Cancellation and tiny positive coefficients that round to A==1 hide the entire section.
 second.m_ArtefactHitImmunities.v[burn]=.1f;actor.inv.m_belt={&art,&second};panel.UpdateProtectionHints(&actor);has(states[stt_fire],"ui_uip_protection_artifact_effect",false);
 art.m_ArtefactHitImmunities.v[burn]=.02f;actor.inv.m_belt={&art};panel.UpdateProtectionHints(&actor);has(states[stt_fire],"ui_uip_protection_artifact_effect",false);
 // A real helmet is still applied by HitOutfitEffect when the outfit UI flag says integrated.
 outfit.bIsHelmetAvaliable=false;panel.UpdateProtectionHints(&actor);has(states[stt_fire],"ui_uip_protection_helmet",true);
 helmet.condition=0;panel.UpdateProtectionHints(&actor);has(states[stt_fire],"ui_uip_protection_helmet",false);helmet.condition=1;
 actor.helmet=nullptr;panel.UpdateProtectionHints(&actor);has(states[stt_fire],"ui_uip_protection_helmet",false);actor.helmet=&helmet;
 outfit.condition=0;panel.UpdateProtectionHints(&actor);has(states[stt_fire],"ui_uip_protection_outfit",false);outfit.condition=1;
 // Distinct light_burn armour copy: show its effective threshold without changing source tracking.
 outfit.m_HitTypeProtection[light]=.9f;panel.UpdateProtectionHints(&actor);has(states[stt_fire],"ui_uip_protection_effective_light_burn",true);
 // Preserve actual radiation/psy observation units and hide inactive history at expiry.
 actor.m_environmental_exposure.Record(rad,.003f,700);actor.m_environmental_exposure.Record(psi,.1f,700);
 actor.cond.m_environmental_damage.Record(psi,.01f,.02f,0,700);actor.cond.m_environmental_damage.Record(rad,0,0,.003f,700);panel.UpdateProtectionHints(&actor);
 has(states[stt_radia],"ui_uip_protection_contamination",false);has(states[stt_radia],"ui_uip_protection_damage",false);
 textEq(row(states[stt_radia],"ui_uip_protection_received_dose"),"0,30 rad");textEq(row(states[stt_psi],"ui_uip_protection_damage"),"2,00%");
 for(int i=0;i<5;++i)assert(states[i].hint.find("ui_uip_")==std::string::npos);
 Device.dwTimeGlobal=1450;panel.UpdateProtectionHints(&actor);for(auto&s:states){has(s,"ui_uip_protection_source",false);has(s,"ui_uip_protection_damage",false);}
 // Invert each clamp, including nonstandard negative layers; do not invent a zero threshold.
 assert(!Protection::EffectiveThreshold(.2f,0,-.01f,1).attainable);
 assert(!Protection::EffectiveThreshold(.2f,-.01f,0,1).attainable);
 eq(Protection::EffectiveThreshold(.2f,-.01f,.02f,1).power,.21f);
 assert(!Protection::EffectiveThreshold(0,0,0,0).attainable);
 assert(!Protection::EffectiveThreshold(0,0,0,NAN).attainable);
 actor.cond.m_fBoostChemicalBurnProtection=-.01f;panel.UpdateProtectionHints(&actor);
 textEq(row(states[stt_acid],"ui_uip_protection_effective"),table.translate("ui_uip_protection_no_threshold").s);
 // Regression for the reported +100 points: S=.02 must not become flat armour.
 actor.inv.m_belt={&art};actor.cond.m_fBoostChemicalBurnProtection=0;
 actor.cond.m_fBoostBurnImmunity=0;actor.outfit=&outfit;actor.helmet=nullptr;
 outfit.condition=1;outfit.m_HitTypeProtection[burn]=.1f;
 art.condition=1;art.m_ArtefactHitImmunities.v[burn]=.02f;
 eq(actor.GetEquipmentProtection(burn),.01f);
 eq(GetEquipmentBurnProtectionRatio()*100,50);
 panel.UpdateProtectionHints(&actor);
 textEq(row(states[stt_fire],"ui_uip_protection_effective"),"50,00 pkt");
 eq(states[stt_fire].protection,GetEquipmentBurnProtectionRatio());
 actor.outfit=nullptr;eq(GetEquipmentBurnProtectionRatio(),0);
 art.m_ArtefactHitImmunities.v[burn]=coefficient;eq(GetEquipmentBurnProtectionRatio(),0);
 actor.outfit=&outfit;eq(GetEquipmentBurnProtectionRatio()*100,66.666667);
 panel.UpdateProtectionHints(&actor);eq(states[stt_fire].protection,GetEquipmentBurnProtectionRatio());
 // All five actual expression delegates stay on the same value as the panel getter.
 float(*ratios[])()={GetEquipmentBurnProtectionRatio,GetEquipmentShockProtectionRatio,GetEquipmentChemicalBurnProtectionRatio,GetEquipmentRadiationProtectionRatio,GetEquipmentTelepaticProtectionRatio};
 const ALife::EHitType panelTypes[]={burn,shock,chem,rad,psi};
 for(int i=0;i<5;++i)eq(ratios[i](),Protection::DisplayRatio(actor.GetEquipmentProtection(panelTypes[i]),actor.cond.GetZoneMaxPower(panelTypes[i])));
 actor.cond.m_fBoostChemicalBurnProtection=.05f;
 eq(GetEquipmentChemicalBurnProtectionRatio(),Protection::DisplayRatio(actor.GetEquipmentProtection(chem),.2f));
 actor.cond.m_fBoostChemicalBurnProtection=-.01f;eq(actor.GetEquipmentProtection(chem),0);
 // Source and tooltip stay aligned throughout periodic hits, including gaps >150 ms.
 actor.m_environmental_exposure.Reset();actor.cond.m_environmental_damage.Reset();
 for(u32 time=2000;time<3250;time+=25){
  Device.dwTimeGlobal=time;
  if((time-2000)%250==0){actor.m_environmental_exposure.Record(burn,.00527f,time);actor.cond.m_environmental_damage.Record(burn,0,0,0,time);}
  panel.UpdateProtectionHints(&actor);
  textEq(row(states[stt_fire],"ui_uip_protection_source"),"26,35 pkt");
  textEq(row(states[stt_fire],"ui_uip_protection_damage"),"0,00%");
  eq(states[stt_fire].peak*100,26.35f);
 }
 Device.dwTimeGlobal=3749;panel.UpdateProtectionHints(&actor);has(states[stt_fire],"ui_uip_protection_source",true);
 Device.dwTimeGlobal=3750;panel.UpdateProtectionHints(&actor);has(states[stt_fire],"ui_uip_protection_source",false);
 // Whole numbers, zeros and decimals in detailed tooltips keep two decimal places.
 string64 fixed;
 ConditionUi::FormatDetailNumber(fixed,25,',',false);textEq(fixed,"25,00");
 ConditionUi::FormatDetailNumber(fixed,0,',',false);textEq(fixed,"0,00");
 ConditionUi::FormatDetailNumber(fixed,-.00000001,',',false);textEq(fixed,"0,00");
 ConditionUi::FormatDetailNumber(fixed,25.5,'.',true);textEq(fixed,"+25.50");
 ConditionUi::FormatDetailNumber(fixed,.001,',',false);textEq(fixed,"<0,01");
 ConditionUi::FormatNumber(fixed,25,',',false);textEq(fixed,"25");
 // Components are fully dim, and labels no longer have column padding.
 actor.helmet=&helmet;panel.UpdateProtectionHints(&actor);
 assert(states[stt_fire].hint.find(std::string(kColSep)+"  "+table.translate("ui_uip_protection_helmet").s+": ")!=std::string::npos);
 assert(states[stt_fire].hint.find("   ")==std::string::npos);
 // Removing a translated feature key is safe, as are absent widgets.
 panel.m_state[stt_fire]=nullptr;panel.UpdateProtectionHints(&actor);
 table.values.erase("ui_uip_protection_effective");panel.UpdateProtectionHints(&actor);
 std::cout<<checks<<" threshold, pipeline and tooltip checks passed\n";
}
'''
with tempfile.TemporaryDirectory(prefix='environmental-protection-') as tmp:
    work=Path(tmp);(work/'test.cpp').write_text(cpp)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-O1','-g','-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all','-no-pie','-I'+str(root/'src/xrGame'),str(work/'test.cpp'),'-o',str(work/'test')],check=True)
    run=subprocess.run([str(work/'test')],check=True,stdout=subprocess.PIPE)
    output=run.stdout.decode('cp1250')
    if args.examples:
        args.examples.mkdir(parents=True,exist_ok=True)
        for name,text in re.findall(r'EXAMPLE (.*?)\n(.*?)\nEND EXAMPLE',output,re.S):
            (args.examples/(name+'.txt')).write_text(text.replace('\\n','\n')+'\n')
    print(output[output.rfind('END EXAMPLE')+len('END EXAMPLE'):].strip())

required=['total','effective','temporary','artifact_effect','no_threshold','effective_light_burn','source','damage','received_dose']
for lang in ['pol','eng','cze','rus']:
    encoding='cp1251' if lang=='rus' else 'cp1250' if lang in ('pol','cze') else 'cp1252'
    mod=strings(args.mod_root/f'configs/text/{lang}/zz_uiparams_panel.xml',encoding)
    engine=strings(root/f'gamedata/configs/text/{lang}/ui_protection_points.xml',encoding)
    for suffix in required:
        key='ui_uip_protection_'+suffix;assert mod[key]==engine[key] and mod[key]
xml=ET.parse(args.mod_root/'configs/ui/mod_actor_menu_16_uiparams.xml')
node=xml.find('.//actor_state_info/environment_hint_wnd');assert node is not None
assert float(node.find('text').get('width'))==256
assert node.find('text/text').get('complex_mode')=='1'
assert node.find('text/text').get('font')=='ui_font_panel_tt'
assert float(node.get('width'))==float(node.find('background').get('width'))
assert 'AdjustHeightToText' in read('src/xrUI/Widgets/UIHint.cpp')
ui=read('src/xrGame/ui/UIActorStateInfo.cpp')
assert 'm_state[state]->set_hint_wnd(m_environment_hint_wnd)' in ui
assert 'delete_data(m_environment_hint_wnd)' in ui and 'm_environment_hint_wnd->Draw()' in ui
print('PL/EN/CZ/RU labels, dedicated tooltip geometry and dynamic height wiring passed')
