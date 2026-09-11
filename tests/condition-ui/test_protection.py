"""Compile production calculation/UI functions with lightweight engine fixtures.

This exercises hit processing, condition, both item comparisons, the panel,
XML delegates and upgrade deltas. It does not render the Windows game.
"""
from pathlib import Path
import argparse
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser()
parser.add_argument('--mod-root', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]

def read(path):
    return (root / path).read_bytes().decode('latin1').replace('\r\n', '\n')

def function(path, signature):
    text = read(path)
    start = text.index(signature)
    begin = text.index('{', start)
    end, depth = begin + 1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end] + '\n'

enums = read('src/xrEngine/AI/alife_space.h')
enums = '\n'.join(re.search(r'enum '+name+r'\s*\{.*?\};', enums, re.S)[0]
                  for name in ['EHitType', 'EInfluenceType', 'EConditionRestoreType'])

cpp = r'''
#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <cstdint>
using u8=uint8_t;using u16=uint16_t;using u32=uint32_t;using s16=int16_t;
using LPCSTR=const char*;using xr_string=std::string;
using string32=char[32];using string64=char[64];using string256=char[256];using string2048=char[2048];
using std::pow;using std::floor;
u32 color_rgba(u32 r,u32 g,u32 b,u32 a){return (a<<24)|(r<<16)|(g<<8)|b;}
struct DeviceFixture{u32 dwTimeGlobal=0;} Device;
template<class T> void clamp(T& v,T lo,T hi){v=std::max(lo,std::min(v,hi));}
bool fis_zero(float v){return std::fabs(v)<1e-6f;}
#define VERIFY(x) assert(x)
#define NODEFAULT assert(false)
#define _max std::max
#define flt_max FLT_MAX
int xr_strcmp(const char*a,const char*b){return std::strcmp(a,b);}
template<size_t N,class...Ts> void xr_strconcat(char(&out)[N],Ts...parts){std::string s;(s.append(parts),...);assert(s.size()<N);std::strcpy(out,s.c_str());}
void xr_strcat(char*out,size_t n,const char*s){assert(std::strlen(out)+std::strlen(s)<n);std::strcat(out,s);}
template<size_t N,class...Ts>void xr_sprintf(char(&out)[N],const char*fmt,Ts...vs){std::snprintf(out,N,fmt,vs...);}
struct shared_str {std::string s;shared_str(const char*v=""):s(v){}const char*c_str()const{return s.c_str();}const std::string&_get()const{return s;}size_t size()const{return s.size();}operator const char*()const{return s.c_str();}};
'''
cpp += 'namespace ALife {\n' + enums + '\n}\n#define XRAY_ALIFE_SPACE\n'
cpp += '#include "ProtectionValues.h"\n#include "EnvironmentalExposure.h"\n#include "EnvironmentalDamage.h"\n#include "ConditionUiValues.h"\n'
cpp += r'''
namespace ConditionUi {
char separator=',';
char DecimalSeparator(){return separator;}
bool RegenerationUnitsEnabled(){return false;}
bool RadiationUnitsEnabled(){return false;}
void FormatRegenerationRate(string64&,float){}
}
'''
cpp += r'''
struct Fvector2 {float x=0,y=0;Fvector2&set(float a,float b){x=a;y=b;return *this;}Fvector2&set(Fvector2 v){*this=v;return *this;}};
struct Widget {std::string text;float cur=0,comp=0,height=18;Fvector2 pos;int color=0;
 void SetText(const char*s){text=s?s:"";}void SetTextColor(int c){color=c;}void SetTextureColor(u32 c){color=int(c);}void InitTexture(const char*){}
 void SetTwoPos(float a,float b){cur=a;comp=b;}void SetWndPos(Fvector2 p){pos=p;}Fvector2 GetWndPos(){return pos;}Fvector2 GetWndSize(){return {214,height};}
 void SetHeight(float h){height=h;}
};
constexpr int red_clr=1,green_clr=2;
struct UIArtefactParamItem:Widget {Widget val,caption;Widget*m_value=&val,*m_caption=&caption;shared_str m_texture_minus,m_texture_plus;
 void SetProtectionRatio(float);void SetValue(float v){cur=v;}void SetCaption(const char*){}void SetRegenerationRate(float){}void SetRadiationRate(float){} };
struct CUIOutfitImmunity {bool m_zone_protection=true;Widget m_progress,value;Widget*m_value=&value;float m_magnitude=100;shared_str m_unit_str;
 void SetProgressValue(float,float);};
struct Settings {std::map<std::string,std::map<std::string,std::string>> data;
 bool section_exist(const char*s){return s&&data.count(s);}bool line_exist(const char*s,const char*k){return section_exist(s)&&data[s].count(k);}
 const char*r_string(const char*s,const char*k){return data.at(s).at(k).c_str();}float r_float(const char*s,const char*k){return std::stof(r_string(s,k));}
 u32 r_u32(const char*s,const char*k){return u32(r_float(s,k));}
} settings;Settings*pSettings=&settings;
#define READ_IF_EXISTS(ini,method,section,key,fallback) ((ini)->line_exist((section).c_str(),key)?(ini)->method((section).c_str(),key):(fallback))
struct CArtefact;
struct CInventoryItem {float condition=1;shared_str m_section_id;float GetCondition(){return condition;}virtual CArtefact*cast_artefact(){return nullptr;}};
struct Immunities {float v[ALife::eHitTypeMax]={};float AffectHit(float power,ALife::EHitType t){return power*v[t];}};
struct CArtefact:CInventoryItem {Immunities m_ArtefactHitImmunities;CArtefact*cast_artefact()override{return this;}bool DegradationRate(){return false;}};
using PIItem=CInventoryItem*;
struct Bones {float m_fHitFracActor=.1f;float getBoneProtection(s16){return 1;}} bones;
bool IsGameTypeSingle(){return true;}
struct CCustomOutfit:CInventoryItem {float m_HitTypeProtection[ALife::eHitTypeMax]={};Bones*m_boneProtection=&bones;bool bIsHelmetAvaliable=true;
 float GetDefHitTypeProtection(ALife::EHitType);float HitThroughArmor(float,s16,float,bool&,ALife::EHitType);
 float GetBoneArmor(s16){return .2f;}void Hit(float,ALife::EHitType){} };
struct CHelmet:CCustomOutfit {float GetDefHitTypeProtection(ALife::EHitType);float HitThroughArmor(float,s16,float,bool&,ALife::EHitType);};
struct CActorCondition {Protection::DamageHistory m_environmental_damage;Protection::DamageReading GetEnvironmentalDamage(ALife::EHitType)const;float m_zone_max_power[5]={.03f,.2f,.2f,.1f,.8f};float m_max_wound_protection=1;
 float GetZoneMaxPower(ALife::EInfluenceType)const;float GetZoneMaxPower(ALife::EHitType)const;float GetMaxFireWoundProtection(){return 1;}};
struct Inventory {std::vector<PIItem>m_belt;};
struct CActor {Protection::ExposureHistory m_environmental_exposure;
 Protection::ExposureReading GetEnvironmentalExposure(ALife::EHitType) const;Inventory inv;CCustomOutfit*outfit=nullptr;CHelmet*helmet=nullptr;CActorCondition cond;
 Inventory&inventory(){return inv;}CCustomOutfit*GetOutfit(){return outfit;}CHelmet*GetHelmet(){return helmet;}
 CActorCondition&conditions(){return cond;}CActor*cast_actor(){return this;}void*Visual(){return nullptr;}
 float HitArtefactsOnBelt(float,ALife::EHitType);float GetProtection_ArtefactsOnBelt(ALife::EHitType);float GetEquipmentProtection(ALife::EHitType);
};
struct IKinematics {u16 LL_BoneID(const char*){return 0;}} ik;
IKinematics* PKinematics(void*){return &ik;}
struct Upgrade {shared_str props[4];std::string sect;shared_str get_property_name(u8 i){return props[i];}const char*section(){return sect.c_str();}};
struct Manager {std::map<std::string,Upgrade>up;Upgrade*get_upgrade(const std::string&id){return &up.at(id);}} manager;
namespace inventory::upgrade {constexpr int max_properties_count=4;}
struct LevelFixture {CActor*actor=nullptr;Manager*m_upgrade_manager=&manager;CActor*CurrentViewEntity(){return actor;}} level;
LevelFixture&Level(){return level;}
CActor*GetActor(){return level.actor;}
struct StringTable {std::map<std::string,std::string>texts;shared_str translate(const char*s){auto i=texts.find(s);return shared_str(i==texts.end()?s:i->second.c_str());}} strings;
StringTable*g_pStringTable=&strings;
struct UIProperty {using ItemUpgrades_type=std::vector<std::string>;using Upgrade_type=Upgrade;shared_str m_property_id;string256 m_text={};Widget text;Widget*m_ui_text=&text;
 UIProperty*get_property(){return this;}bool read_value_from_section(LPCSTR,LPCSTR,float&);bool compute_value(const ItemUpgrades_type&);
 ALife::EHitType protection_type()const;bool is_protection()const;bool show_result(const char*){text.text="legacy";return true;}};
struct CUIOutfitInfo {static constexpr u32 max_count=ALife::eHitTypeMax-2;CUIOutfitImmunity*m_items[max_count]={};
 void UpdateInfo(CCustomOutfit*,CCustomOutfit*);void UpdateInfo(CHelmet*,CHelmet*);};
enum EStateType {stt_main,stt_fire,stt_shock,stt_acid,stt_radia,stt_psi,stt_wound};
struct OverflowMarker {bool shown=false;void Show(bool value){shown=value;}};
struct ExposureProgress {Widget m_UIProgressItem;float pos=0;bool shown=false;
 void Show(bool value){shown=value;}void SetProgressPosImmediate(float value){pos=std::clamp(value,0.f,1.f);}};
struct ui_actor_state_item {ExposureProgress sourceBar,protectionBar;ExposureProgress*m_environmental_exposure=&sourceBar,*m_progress=&protectionBar;
 void set_environmental_exposure(ALife::EHitType,float,float,float);OverflowMarker frame,triangle;OverflowMarker*m_overflow=&frame,*m_overflow_fill=&triangle;
 void set_protection_overflow(float);float fill=0,arrow=0;std::string text,hint;void set_hint_text(const char*s){hint=s;}bool set_progress(float f){fill=f;return true;}void set_arrow(float f){arrow=f;}void set_text(float){}void set_text_str(const char*s){text=s;}};
using State=ui_actor_state_item;
struct ui_actor_state_wnd {State*m_state[7];void update_round_states(EStateType,float,float);void UpdateProtectionHints(CActor*);};
struct CUIArtefactParams:Widget {UIArtefactParamItem*m_immunity_item[ALife::eHitTypeWound_2]={},*m_restore_item[ALife::eRestoreTypeMax]={};
 Widget*m_Prop_line=nullptr;UIArtefactParamItem*m_disp_condition=nullptr,*m_additional_weight=nullptr,*m_af_slots=nullptr;
 bool is_artefact(){return true;}bool is_backpack(){return false;}void DetachAll(){}void AttachChild(Widget*){}void SetInfo(CInventoryItem&);};
'''
cpp += 'namespace ConditionUi {\n' + function('src/xrGame/ui/UIConditionFormat.h', '    inline void FormatProtectionPoints') + '}\n'
# Use the production hint style constants, including literal backslash-n breaks.
style=read('src/xrGame/ui/UIActorStateInfo.cpp')
for name in ['kColTitle','kColSep','kBreak']:
    cpp += re.search(r'static LPCSTR '+name+r'\s*=.*?;',style)[0]+'\n'
for file, signature in [
    ('Actor.cpp', 'Protection::ExposureReading CActor::GetEnvironmentalExposure'),
    ('ActorCondition.cpp', 'Protection::DamageReading CActorCondition::GetEnvironmentalDamage'),
    ('ui/UIActorStateInfo.cpp', 'void ui_actor_state_item::set_environmental_exposure'),
    ('ui/UIActorStateInfo.cpp', 'void ui_actor_state_item::set_protection_overflow'),
    ('Actor.cpp', 'float CActor::HitArtefactsOnBelt'),
    ('Actor.cpp', 'float CActor::GetProtection_ArtefactsOnBelt'),
    ('Actor.cpp', 'float CActor::GetEquipmentProtection'),
    ('CustomOutfit.cpp', 'float CCustomOutfit::GetDefHitTypeProtection'),
    ('ActorHelmet.cpp', 'float CHelmet::GetDefHitTypeProtection'),
    ('CustomOutfit.cpp', 'float CCustomOutfit::HitThroughArmor'),
    ('ActorHelmet.cpp', 'float CHelmet::HitThroughArmor'),
    ('ActorCondition.cpp', 'float CActorCondition::GetZoneMaxPower( ALife::EInfluenceType'),
    ('ActorCondition.cpp', 'float CActorCondition::GetZoneMaxPower( ALife::EHitType'),
    ('ui/UIOutfitInfo.cpp', 'void CUIOutfitImmunity::SetProgressValue'),
    ('ui/UIOutfitInfo.cpp', 'void CUIOutfitInfo::UpdateInfo(CCustomOutfit*'),
    ('ui/UIOutfitInfo.cpp', 'void CUIOutfitInfo::UpdateInfo(CHelmet*'),
    ('ui/UIInvUpgradeProperty.cpp', 'ALife::EHitType UIProperty::protection_type'),
    ('ui/UIInvUpgradeProperty.cpp', 'bool UIProperty::is_protection'),
    ('ui/UIInvUpgradeProperty.cpp', 'bool UIProperty::read_value_from_section'),
    ('ui/UIInvUpgradeProperty.cpp', 'bool UIProperty::compute_value'),
    ('ui/ui_af_params.cpp', 'void UIArtefactParamItem::SetProtectionRatio'),
    ('ui/UIActorStateInfo.cpp', 'void ui_actor_state_wnd::update_round_states'),
    ('ui/UIActorStateInfo.cpp', 'void ui_actor_state_wnd::UpdateProtectionHints'),
]:
    cpp += function('src/xrGame/'+file, signature)

af = read('src/xrGame/ui/ui_af_params.cpp')
for name in ['af_immunity_section_names','af_restore_section_names','af_zone_hit_types']:
    cpp += re.search(r'[^\n]*\b'+name+r'\[\].*?\};',af,re.S)[0] + '\n'
cpp += function('src/xrGame/ui/ui_af_params.cpp', 'void CUIArtefactParams::SetInfo')
delegates = read('src/xrGame/game_expression_delegates.cpp')
cpp += delegates[delegates.index('#define DECLARE_EQUIPMENT_PROTECTION_RATIO'):delegates.index('#undef DECLARE_EQUIPMENT_PROTECTION_RATIO')] + '\n'
cpp += r'''
int checks=0;
void eq(double a,double b,const char*label){++checks;if(std::abs(a-b)>1e-6+std::abs(b)*1e-5){std::cerr<<label<<": "<<a<<" != "<<b<<"\n";std::abort();}}
void textEq(const std::string&a,const std::string&b){++checks;if(a!=b){std::cerr<<a<<" != "<<b<<"\n";std::abort();}}
int main(){
 strings.texts["ui_uip_unit_protection"]="pkt";
 CActor a;level.actor=&a;CCustomOutfit outfit;CHelmet helmet;CArtefact af1,af2;CInventoryItem unrelated;
 a.inv.m_belt={&af1,&unrelated,&af2};a.outfit=&outfit;a.helmet=&helmet;
 CUIOutfitInfo outfitUI;CUIOutfitImmunity outfitRows[9];CUIArtefactParams afUI;UIArtefactParamItem afRows[5];
 for(int i=0;i<5;++i){outfitUI.m_items[i]=&outfitRows[i];afUI.m_immunity_item[i]=&afRows[i];}
 State states[7];ui_actor_state_wnd panel;for(int i=0;i<7;++i)panel.m_state[i]=&states[i];
 const EStateType statesForHit[]={stt_fire,stt_shock,stt_acid,stt_radia,stt_psi};
 const char*props[]={"prop_thermo","prop_electro","prop_chem","prop_radio","prop_psy"};
 const int afRowsForHit[]={1,4,2,0,3};
 float(*ratios[])()={GetEquipmentBurnProtectionRatio,GetEquipmentShockProtectionRatio,GetEquipmentChemicalBurnProtectionRatio,GetEquipmentRadiationProtectionRatio,GetEquipmentTelepaticProtectionRatio};
 // Independent expected model, crossed with real actor, armour and tooltip code.
 for(int t=0;t<5;++t){auto type=ALife::EHitType(t);
  for(float maximum:{.03f,.2f,.8f})for(float condition:{0.f,.01f,.5f,1.f})for(float resistance:{-.04f,0.f,.02f,.04f,1.2f}){
   for(auto&v:a.cond.m_zone_max_power)v=maximum;
   af1.m_ArtefactHitImmunities.v[t]=resistance;af1.condition=condition;
   af2.m_ArtefactHitImmunities.v[t]=.012f;af2.condition=.4f;
   outfit.m_HitTypeProtection[t]=.3f;outfit.condition=.5f;helmet.m_HitTypeProtection[t]=.2f;helmet.condition=.25f;
   const float expectedAf=resistance*condition+.012f*.4f;
   const float expectedOutfit=.3f*.5f*.1f,expectedHelmet=.2f*.25f*.1f;
   const float total=expectedAf+expectedOutfit+expectedHelmet;
   eq(a.GetProtection_ArtefactsOnBelt(type),expectedAf,"condition-weighted belt sum");
   eq(a.GetEquipmentProtection(type),total,"equipment sum; condition once");
   eq(ratios[t](),total/maximum*10,"XML delegate matches panel");
   for(float hit:{0.f,.001f,.02f,.2f,1.f,3.f}){
    bool wound=true;float afterAf=a.HitArtefactsOnBelt(hit,type);
    eq(afterAf,std::max(0.f,hit-expectedAf),"absolute artefact subtraction");
    float after=outfit.HitThroughArmor(afterAf,0,0,wound,type);
    after=helmet.HitThroughArmor(after,0,0,wound,type);
    eq(after,std::max(0.f,hit-total),"combined hit vs independent formula");
   }
   CCustomOutfit comparison;comparison.m_HitTypeProtection[t]=.8f;comparison.condition=.25f;
   outfitUI.UpdateInfo(&outfit,&comparison);
   eq(outfitRows[t].m_progress.cur,std::clamp(expectedOutfit/maximum*10,0.f,1.f)*100,"outfit comparison current");
   eq(outfitRows[t].m_progress.comp,std::clamp(.8f*.25f*.1f/maximum*10,0.f,1.f)*100,"outfit comparison slot");
   string64 text;ConditionUi::FormatProtectionPoints(text,expectedOutfit/maximum*10);textEq(outfitRows[t].value.text,text);
   CHelmet comparisonHelmet;comparisonHelmet.m_HitTypeProtection[t]=.6f;comparisonHelmet.condition=.1f;
   outfitUI.UpdateInfo(&helmet,&comparisonHelmet);
   eq(outfitRows[t].m_progress.cur,std::clamp(expectedHelmet/maximum*10,0.f,1.f)*100,"helmet current");
   eq(outfitRows[t].m_progress.comp,std::clamp(.6f*.1f*.1f/maximum*10,0.f,1.f)*100,"helmet comparison");
   panel.update_round_states(statesForHit[t],total,maximum);
   assert(states[statesForHit[t]].frame.shown==(total/maximum*10>1));
   assert(states[statesForHit[t]].triangle.shown==(total/maximum*10>1));
   eq(states[statesForHit[t]].fill,std::clamp(total/maximum*10,0.f,1.f),"only graphical fill clamped");
   ConditionUi::FormatProtectionPoints(text,total/maximum*10,false);textEq(states[statesForHit[t]].text,text);
  }
 }
 // Actual artefact SetInfo: distinguish hit enum order from influence enum order.
 a.cond.m_zone_max_power[0]=.03f;a.cond.m_zone_max_power[1]=.2f;a.cond.m_zone_max_power[2]=.4f;a.cond.m_zone_max_power[3]=.1f;a.cond.m_zone_max_power[4]=.8f;
 af1.m_section_id="af_test";af1.condition=.5f;settings.data["af_test"]["hit_absorbation_sect"]="af_abs";
 for(const char*k:af_immunity_section_names)settings.data["af_abs"][k]="0.04";
 afUI.SetInfo(af1);
 for(int t=0;t<5;++t){string64 text;ConditionUi::FormatProtectionPoints(text,.2f/a.conditions().GetZoneMaxPower(ALife::EHitType(t)));textEq(afRows[afRowsForHit[t]].val.text,text);}
 textEq(afRows[1].val.text,"+100 pkt");textEq(afRows[4].val.text,"+25 pkt");
 // Null equipment, unloaded actor and heat alias.
 a.inv.m_belt.clear();a.outfit=nullptr;a.helmet=nullptr;eq(a.GetEquipmentProtection(ALife::eHitTypeBurn),0,"empty equipment");
 level.actor=nullptr;eq(GetEquipmentBurnProtectionRatio(),0,"no actor");level.actor=&a;
 af1.condition=.01f;af1.m_ArtefactHitImmunities.v[ALife::eHitTypeLightBurn]=.04f;a.inv.m_belt={&af1};
 eq(a.HitArtefactsOnBelt(.2f,ALife::eHitTypeLightBurn),.1996f,"ambient heat at 1% condition");
 for(auto type:{ALife::eHitTypeWound,ALife::eHitTypeStrike,ALife::eHitTypeExplosion,ALife::eHitTypeFireWound,ALife::eHitTypeWound_2}){
  af1.condition=1;af1.m_ArtefactHitImmunities.v[type]=.1f;eq(a.HitArtefactsOnBelt(1,type),.977828695f,"other damage mechanics unchanged");
  af1.m_ArtefactHitImmunities.v[type]=-.1f;eq(a.HitArtefactsOnBelt(1,type),1.022171305f,"other damage penalties unchanged");
 }
 // Tooltip numbers are neither XML magnitudes nor clamped percentages.
 string64 text;ConditionUi::FormatProtectionPoints(text,1.23456f);textEq(text,"+123,46 pkt");
 ConditionUi::FormatProtectionPoints(text,-.0125f);textEq(text,"-1,25 pkt");
 ConditionUi::FormatProtectionPoints(text,.000001f);textEq(text,"+<0,01 pkt");
 ConditionUi::FormatProtectionPoints(text,0);textEq(text,"0 pkt");
 CUIOutfitImmunity row;row.SetProgressValue(1.5f,-.2f);eq(row.m_progress.cur,100,"tooltip fill max");eq(row.m_progress.comp,0,"tooltip fill min");textEq(row.value.text,"+150 pkt");
 ConditionUi::separator='.';ConditionUi::FormatProtectionPoints(text,.125f);textEq(text,"+12.5 pkt");ConditionUi::separator=',';
 // Upgrade uses actual effect section, not misleading static value; no condition.
 for(int t=0;t<5;++t){auto type=ALife::EHitType(t);for(auto&v:a.cond.m_zone_max_power)v=.2f;
  UIProperty p;p.m_property_id=props[t];manager.up.clear();settings.data.clear();
  Upgrade one;one.props[0]=props[t];one.sect="effect1";manager.up["one"]=one;
  Upgrade two=one;two.sect="effect2";manager.up["two"]=two;
  settings.data["effect1"][Protection::ConfigKey(type)]="0.04";settings.data["effect1"]["value"]="999";
  settings.data["effect2"][Protection::ConfigKey(type)]="-0.01";settings.data["effect2"]["value"]="100";
  assert(p.compute_value({"one"}));textEq(p.text.text,std::string(Protection::Caption(type))+": +20 pkt");
  assert(p.compute_value({"one","two"}));textEq(p.text.text,std::string(Protection::Caption(type))+": +15 pkt");
  assert(p.compute_value({"two"}));textEq(p.text.text,std::string(Protection::Caption(type))+": -5 pkt");
  settings.data["effect1"].erase(Protection::ConfigKey(type));assert(!p.compute_value({"one"}));
  settings.data["effect1"][Protection::ConfigKey(type)]="0";assert(p.compute_value({"one"}));textEq(p.text.text,std::string(Protection::Caption(type))+": 0 pkt");
  level.actor=nullptr;assert(!p.compute_value({"one"}));level.actor=&a;
 }

 // All five rows: strict boundary, repeated increase/decrease, and uncapped numbers.
 for(auto state:statesForHit)for(float ratio:{0.f, .9999f, 1.f, 1.0001f, 2.5f, 1.f, .5f, -1.f}){
  panel.update_round_states(state,ratio,10.f);
  assert(states[state].frame.shown==(ratio>1));
  assert(states[state].triangle.shown==(ratio>1));
  eq(states[state].fill,std::clamp(ratio,0.f,1.f),"overflow leaves bar clamped");
  string64 expected;ConditionUi::FormatProtectionPoints(expected,ratio,false);
  textEq(states[state].text,expected);
 }
 State noMarker;noMarker.m_overflow=nullptr;noMarker.m_overflow_fill=nullptr;
 noMarker.set_protection_overflow(2.5f);noMarker.set_protection_overflow(0.f);
 // Regression: real STCoP outfit and fireball values, full and screenshot condition.
 a.inv.m_belt.clear();a.outfit=&outfit;a.helmet=nullptr;
 a.cond.m_zone_max_power[ALife::infl_fire]=.2f;
 outfit.m_HitTypeProtection[ALife::eHitTypeBurn]=.1f;outfit.condition=1;
 eq(GetEquipmentBurnProtectionRatio()*100,50,"Monolith thermal score");
 outfit.condition=.906f;eq(GetEquipmentBurnProtectionRatio()*100,45.3,"screenshot Monolith condition");
 outfit.m_HitTypeProtection[ALife::eHitTypeBurn]=.065f;outfit.condition=1;
 eq(GetEquipmentBurnProtectionRatio()*100,32.5,"Sunrise thermal score");
 outfit.condition=.926f;eq(GetEquipmentBurnProtectionRatio()*100,30.095,"screenshot Sunrise condition");
 outfit.condition=1;outfit.m_HitTypeProtection[ALife::eHitTypeBurn]=.1f;
 af1.condition=1;af1.m_ArtefactHitImmunities.v[ALife::eHitTypeBurn]=.04f;a.inv.m_belt={&af1};
 eq(GetEquipmentBurnProtectionRatio()*100,250,"Monolith plus Fireball: no numeric cap");
 panel.update_round_states(stt_fire,a.GetEquipmentProtection(ALife::eHitTypeBurn),.2f);
 assert(states[stt_fire].triangle.shown);textEq(states[stt_fire].text,"250 pkt");
 a.inv.m_belt.clear();panel.update_round_states(stt_fire,a.GetEquipmentProtection(ALife::eHitTypeBurn),.2f);
 assert(!states[stt_fire].triangle.shown);textEq(states[stt_fire].text,"50 pkt");
 a.inv.m_belt={&af1};
 strings.texts["ui_uip_protection_description"]="POINTS_DESCRIPTION";
 panel.UpdateProtectionHints(&a);
 const std::string hint=states[stt_fire].hint;
 assert(hint.find(std::string(1,char(92))+"n")!=std::string::npos);
 assert(hint.find(char(10))==std::string::npos);
 assert(hint.find("ui_uip_protection_total: %c[255,224,230,234]250 pkt")!=std::string::npos);
 assert(hint.find("ui_uip_protection_outfit: %c[255,224,230,234]50 pkt")!=std::string::npos);
 assert(hint.find("ui_uip_protection_helmet")==std::string::npos);
 assert(hint.find("ui_uip_protection_artefacts: %c[255,224,230,234]200 pkt")!=std::string::npos);
 assert(hint.find("POINTS_DESCRIPTION")!=std::string::npos);
 const std::string heading=std::string(kColTitle)+Protection::Caption(ALife::eHitTypeBurn)+kBreak+kBreak+kColSep+". . . . . . . . . . . . . .";
 assert(hint.rfind(heading,0)==0);
 // Rows follow equipped objects, not nonzero protection in the selected channel.
 af1.m_ArtefactHitImmunities.v[ALife::eHitTypeBurn]=0;
 panel.UpdateProtectionHints(&a);
 assert(states[stt_fire].hint.find("ui_uip_protection_artefacts: %c[255,224,230,234]0 pkt")!=std::string::npos);
 a.helmet=&helmet;outfit.bIsHelmetAvaliable=true;
 panel.UpdateProtectionHints(&a);
 assert(states[stt_fire].hint.find("ui_uip_protection_helmet")!=std::string::npos);
 outfit.bIsHelmetAvaliable=false;panel.UpdateProtectionHints(&a);
 assert(states[stt_fire].hint.find("ui_uip_protection_helmet")==std::string::npos);
 a.helmet=nullptr;outfit.bIsHelmetAvaliable=true;

 af1.m_ArtefactHitImmunities.v[ALife::eHitTypeBurn]=-.04f;
 panel.UpdateProtectionHints(&a);
 assert(states[stt_fire].hint.find("-200 pkt")!=std::string::npos);
 a.outfit=nullptr;a.inv.m_belt.clear();panel.UpdateProtectionHints(&a);
 assert(states[stt_fire].hint.find("ui_uip_protection_total: %c[255,224,230,234]0 pkt")!=std::string::npos);
 for(auto state:statesForHit){
  assert(states[state].hint.find("ui_uip_protection_outfit")==std::string::npos);
  assert(states[state].hint.find("ui_uip_protection_helmet")==std::string::npos);
  assert(states[state].hint.find("ui_uip_protection_artefacts")==std::string::npos);
 }
 // Non-artefact belt items do not create an artefact row; a standalone helmet does.
 a.inv.m_belt={&unrelated};a.helmet=&helmet;panel.UpdateProtectionHints(&a);
 assert(states[stt_fire].hint.find("ui_uip_protection_artefacts")==std::string::npos);
 assert(states[stt_fire].hint.find("ui_uip_protection_outfit")==std::string::npos);
 assert(states[stt_fire].hint.find("ui_uip_protection_helmet")!=std::string::npos);

 // Incoming exposure is retained independently of equipment and actual damage.
 a.helmet=nullptr;a.outfit=&outfit;outfit.condition=1;outfit.m_HitTypeProtection[ALife::eHitTypeBurn]=.1f;
 a.inv.m_belt.clear();a.m_environmental_exposure.Reset();Device.dwTimeGlobal=1000;
 a.m_environmental_exposure.Record(ALife::eHitTypeLightBurn,.006f,Device.dwTimeGlobal);
 panel.UpdateProtectionHints(&a);
 assert(states[stt_fire].sourceBar.shown);
 eq(states[stt_fire].sourceBar.pos,.3f,"incoming 30 points over 50 protection");
 eq(states[stt_fire].protectionBar.pos,.5f,"equipment layer has matching scale");
 assert(u32(states[stt_fire].sourceBar.m_UIProgressItem.color)==color_rgba(255,160,80,255));
 assert(states[stt_fire].hint.find("ui_uip_protection_source: %c[255,224,230,234]30 pkt")!=std::string::npos);
 eq(a.HitArtefactsOnBelt(.006f,ALife::eHitTypeBurn),.006f,"observation never modifies hit");
 bool wound=true;eq(outfit.HitThroughArmor(.006f,0,0,wound,ALife::eHitTypeBurn),0,"fully protected hit still displayed");
 Device.dwTimeGlobal=1100;a.m_environmental_exposure.Record(ALife::eHitTypeBurn,.016f,Device.dwTimeGlobal);
 panel.UpdateProtectionHints(&a);eq(states[stt_fire].sourceBar.pos,.8f,"incoming 80 points exceeds 50 protection");
 assert(u32(states[stt_fire].sourceBar.m_UIProgressItem.color)==color_rgba(255,160,80,255));
 // Type colors remain stable both below and above protection and the cap.
 af1.condition=1;af1.m_ArtefactHitImmunities.v[ALife::eHitTypeBurn]=.039f;a.inv.m_belt={&af1};
 a.m_environmental_exposure.Reset();Device.dwTimeGlobal=2000;
 a.m_environmental_exposure.Record(ALife::eHitTypeBurn,.036f,Device.dwTimeGlobal);
 panel.UpdateProtectionHints(&a);
 eq(states[stt_fire].sourceBar.pos,1,"source above graphical cap");
 eq(states[stt_fire].protectionBar.pos,1,"protection above graphical cap");
 assert(u32(states[stt_fire].sourceBar.m_UIProgressItem.color)==color_rgba(255,160,80,255));
 assert(states[stt_fire].hint.find("245 pkt")!=std::string::npos);
 assert(states[stt_fire].hint.find("180 pkt")!=std::string::npos);
 Device.dwTimeGlobal=2100;a.m_environmental_exposure.Record(ALife::eHitTypeBurn,.06f,Device.dwTimeGlobal);
 panel.UpdateProtectionHints(&a);
 assert(u32(states[stt_fire].sourceBar.m_UIProgressItem.color)==color_rgba(255,160,80,255));
 assert(states[stt_fire].hint.find("300 pkt")!=std::string::npos);
 Device.dwTimeGlobal=2750;panel.UpdateProtectionHints(&a);
 assert(states[stt_fire].sourceBar.shown);
 assert((u32(states[stt_fire].sourceBar.m_UIProgressItem.color)>>24)<255);
 Device.dwTimeGlobal=2850;panel.UpdateProtectionHints(&a);
 assert(!states[stt_fire].sourceBar.shown);
 assert(states[stt_fire].hint.find("ui_uip_protection_source: %c[255,224,230,234]0 pkt")!=std::string::npos);
 State unsupported;unsupported.m_environmental_exposure=nullptr;unsupported.m_progress=nullptr;
 unsupported.set_environmental_exposure(ALife::eHitTypeBurn,3,2,1);
 Protection::ExposureHistory history;
 const auto burn=ALife::eHitTypeBurn;
 eq(history.Get(burn,0).power,0,"empty history");
 for(auto type:{ALife::eHitTypeBurn,ALife::eHitTypeShock,ALife::eHitTypeChemicalBurn,ALife::eHitTypeRadiation,ALife::eHitTypeTelepatic}){
  history.Reset();history.Record(type,.03f,0);
  eq(history.Get(type,0).power,.03f,"all environmental channels");
  eq(history.Get(type,500).opacity,1,"peak hold boundary");
  eq(history.Get(type,625).opacity,.5f,"fade changes opacity, not strength");
  eq(history.Get(type,625).power,.03f,"strength does not decay into false safety");
  eq(history.Get(type,750).power,0,"expiry boundary");
 }
 history.Reset();history.Record(burn,.2f,0);history.Record(burn,.01f,400);
 eq(history.Get(burn,600).power,.2f,"weak hits do not erase recent strong pulse");
 eq(history.Get(burn,751).power,.01f,"weak ongoing exposure remains after pulse");
 history.Reset();history.Record(ALife::eHitTypeLightBurn,.04f,100);
 eq(history.Get(burn,100).power,.04f,"ambient heat shares thermal channel");
 eq(history.Get(ALife::eHitTypeShock,100).power,0,"channels stay independent");
 history.Record(ALife::eHitTypeShock,.1f,100);eq(history.Get(burn,100).power,.04f,"independent shock record");
 for(auto type:{ALife::eHitTypeWound,ALife::eHitTypeExplosion,ALife::eHitTypeMax}){
  history.Record(type,1,100);eq(history.Get(type,100).power,0,"ignore non-environmental types");
 }
 history.Reset();history.Record(burn,0,0);history.Record(burn,-1,0);
 history.Record(burn,std::nanf(""),0);history.Record(burn,INFINITY,0);
 eq(history.Get(burn,0).power,0,"invalid readings ignored");
 history.Record(burn,.1f,1000);eq(history.Get(burn,10).power,0,"clock rollback has no stale exposure");
 history.Reset();history.Record(burn,.1f,UINT32_MAX-100);
 eq(history.Get(burn,100).power,.1f,"clock wrap preserves recent sample");
 history.Reset();history.Record(burn,.2f,0);
 for(u32 t=1;t<2000;++t)history.Record(burn,.01f,t);
 eq(history.Get(burn,2000).power,.01f,"bounded history under many hits");
 eq(history.Get(burn,2000).opacity,1,"equal recent samples stay bright");
 eq(history.Get(burn,2000).power,history.Get(burn,2000).power,"paused clock holds reading");
 history.Reset();eq(history.Get(burn,2000).power,0,"new life clears history");

 // Type-specific colors at both sides of the protection threshold.
 const ALife::EHitType types[]={burn,ALife::eHitTypeChemicalBurn,ALife::eHitTypeShock,ALife::eHitTypeTelepatic,ALife::eHitTypeRadiation};
 const u32 colors[]={color_rgba(255,160,80,255),color_rgba(170,235,140,255),color_rgba(130,210,255,255),color_rgba(205,165,250,255),color_rgba(245,230,120,255)};
 for(unsigned i=0;i<5;++i)for(float source:{.2f,3.f}){
  states[stt_fire].set_environmental_exposure(types[i],source,.5f,1);
  assert(u32(states[stt_fire].sourceBar.m_UIProgressItem.color)==colors[i]);
 }
 // Actual damage uses resources, never the protection-point scale or time factor.
 Device.dwTimeGlobal=5000;
 a.cond.m_environmental_damage.Record(burn,.075f,0,0,5000);
 a.cond.m_environmental_damage.Record(ALife::eHitTypeTelepatic,.01f,.08f,0,5000);
 a.cond.m_environmental_damage.Record(ALife::eHitTypeRadiation,0,0,.024f,5000);
 panel.UpdateProtectionHints(&a);
 assert(states[stt_fire].hint.find("ui_uip_damage_health: %c[255,224,230,234]7,5%")!=std::string::npos);
 assert(states[stt_fire].hint.find("ui_uip_damage_psy")==std::string::npos);
 assert(states[stt_psi].hint.find("ui_uip_damage_psy: %c[255,224,230,234]8%")!=std::string::npos);
 assert(states[stt_psi].hint.find("ui_uip_damage_health: %c[255,224,230,234]1%")!=std::string::npos);
 assert(states[stt_radia].hint.find("ui_uip_damage_radiation: %c[255,224,230,234]2,4 kBq")!=std::string::npos);
 assert(states[stt_radia].hint.find("ui_uip_damage_health")==std::string::npos);
 a.cond.m_environmental_damage.Record(burn,0,0,0,5001);Device.dwTimeGlobal=5001;panel.UpdateProtectionHints(&a);
 assert(states[stt_fire].hint.find("ui_uip_damage_health: %c[255,224,230,234]0%")!=std::string::npos);
 Device.dwTimeGlobal=8001;panel.UpdateProtectionHints(&a);
 for(auto state:{stt_fire,stt_psi,stt_radia})assert(states[state].hint.find("ui_uip_damage_")==std::string::npos);

 a.outfit=nullptr;a.inv.m_belt.clear();a.m_environmental_exposure.Reset();
 panel.m_state[stt_fire]=nullptr;panel.UpdateProtectionHints(&a);
 eq(Protection::DisplayRatio(.1f,0),0,"invalid reference has finite score");
 eq(Protection::DisplayRatio(.1f,-1),0,"negative reference has finite score");
 strings.texts.erase("ui_uip_unit_protection");
 ConditionUi::FormatProtectionPoints(text,.5f);textEq(text,"+50 pt");

 std::cout<<checks<<" production-function checks passed\n";
}
'''

with tempfile.TemporaryDirectory(prefix='ixray-protection-') as d:
    work = Path(d)
    (work/'test.cpp').write_text(cpp)
    subprocess.run(['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined',
                    '-I'+str(root/'src/xrGame'),str(work/'test.cpp'),'-o',str(work/'test')],check=True)
    subprocess.run([str(work/'test')],check=True)

# Inspect actual UI wiring rather than assuming C++ owns expression-driven bars.
widgets = dict(zip(['fire','shock','acid','radia','psi'],['Burn','Shock','ChemicalBurn','Radiation','Telepatic']))
for p in [root/'gamedata/configs/ui/actor_menu.xml',root/'gamedata/configs/ui/actor_menu_16.xml',args.mod_root/'configs/ui/mod_actor_menu_16_uiparams.xml']:
    tree=ET.parse(p)
    for widget,kind in widgets.items():
        bar=tree.find('.//'+widget+'_sensor/state_progress')
        assert bar is not None
        assert bar.get('expression')=='fltActor'+kind+'ProtectionRatio'
        assert bar.get('min')=='0' and bar.get('max')=='1'
        assert 'RegisterVariable("fltActor'+kind+'ProtectionRatio"' in delegates
    if p.name.startswith('mod_'):
        for widget in widgets:
            row=tree.find('.//'+widget+'_sensor')
            bar=row.find('state_progress')
            source=row.find('source_progress')
            assert source is not None and source.get('expression') is None
            expected={'fire':(255,160,80),'acid':(170,235,140),'shock':(130,210,255),'psi':(205,165,250),'radia':(245,230,120)}[widget]
            assert tuple(int(source.find('progress/texture').get(c)) for c in ['r','g','b'])==expected
            for attr in ['x','width','min','max']:
                assert source.get(attr)==bar.get(attr)
            assert float(source.get('y'))>float(bar.get('y'))
            assert float(source.get('y'))+float(source.get('height'))<float(bar.get('y'))+float(bar.get('height'))

            for tag in ['overflow','overflow_fill']:
                mark=row.find(tag)
                assert mark is not None and mark.get('expression') is None
                assert mark.findtext('texture')=='ui_uiparams_'+('overflow_frame' if tag=='overflow' else tag)
                assert float(mark.get('x'))>float(bar.get('x'))+float(bar.get('width'))
                field=tree.find('.//auto_static[@expression="fltActor'+widgets[widget]+'ProtectionRatio * 100.0"]')
                marker_right=float(row.get('x'))+float(mark.get('x'))+float(mark.get('width'))
                assert marker_right<float(field.get('x'))
        for kind in widgets.values():
            fields=tree.findall('.//auto_static[@expression="fltActor'+kind+'ProtectionRatio * 100.0"]')
            assert len(fields)==1 and fields[0].get('suffix')==''

for suffix in ['', '_16']:
    for p in [root/f'gamedata/configs/ui/af_params{suffix}.xml',args.mod_root/f'configs/ui/mod_af_params{suffix}_uiparams.xml']:
        tree=ET.parse(p)
        for kind in ['burn','shock','chemical_burn','radiation','telepatic']:
            value=tree.find('./'+kind+'_immunity/value')
            assert value.get('magnitude')=='100' and value.get('unit_str')=='ui_uip_unit_protection'

ids=[]
for lang in ['pol','eng','cze','rus']:
    p=args.mod_root/f'configs/text/{lang}/zz_uiparams_panel.xml'
    data=p.read_bytes().decode('cp1250' if lang in ('pol','cze') else 'cp1251')
    tree=ET.fromstring(data[data.index('?>')+2:])
    current=[e.get('id') for e in tree.findall('string')]
    assert len(current)==len(set(current))
    ids.append(set(current))
    for kind in ['burn','shock','chemical_burn','radiation','telepatic']:
        assert f'ui_inv_outfit_{kind}_protection' in current
assert all(s==ids[0] for s in ids)
print('Panel/delegate wiring, five item rows, and PL/EN/CZ/RU text checks passed')

# Check the actual hit/lifecycle wiring, not merely the observation class.
actor=read('src/xrGame/Actor.cpp')
hit=function('src/xrGame/Actor.cpp','void\tCActor::Hit(SHit* pHDS)')
assert hit.index('m_environmental_exposure.Record(HDS.hit_type, HDS.damage(), Device.dwTimeGlobal)')<hit.index('HitArtefactsOnBelt(')
assert actor.count('m_environmental_exposure.Reset();')==1
assert 'm_environmental_exposure.Reset();' in function('src/xrGame/Actor.cpp','void CActor::reinit')
assert 'm_environmental_exposure.Reset();' in function('src/xrGame/Actor_Network.cpp','BOOL CActor::net_Spawn')
print('Incoming hit wiring, lifecycle reset, exposure history, UI layers and source hints passed')
