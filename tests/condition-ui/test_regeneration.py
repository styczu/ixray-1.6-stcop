from pathlib import Path
import subprocess, re, json, xml.etree.ElementTree as ET
import argparse, tempfile
parser=argparse.ArgumentParser()
parser.add_argument('--mod-root',type=Path,required=True)
args=parser.parse_args()
E=Path(__file__).resolve().parents[2]
S=E
def function(file,signature,staged=False):
 text=((S if staged else E)/file).read_bytes().decode('latin1')
 start=text.index(signature); begin=text.index('{',start); level=1; end=begin+1
 while level:
  level += (text[end]=='{')-(text[end]=='}'); end+=1
 return text[start:end].replace('\r\n','\n')

harness=r'''
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <cassert>
#include "ConditionUiValues.h"
using namespace std;
struct Flags { int bits=0; bool test(int mask) const {return bits&mask;} } psActorFlags;
enum {AF_GODMODE=1, AF_DISABLE_CONDITION_TEST=2, mcAnyMove=4};
bool single=true, harmed=true, god=false;
bool IsGameTypeSingleCompatible(){return single;}
bool IsGameTypeSingle(){return single;}
bool GodMode(){return god;}
template<class T> void clamp(T& x,T lo,T hi){x=max(lo,min(x,hi));}
bool fis_zero(float x){return abs(x)<1e-6;}
bool _valid(float x){return isfinite(x);}
#define VERIFY(x) assert(x)
#define _max std::max
using BOOL=bool;
struct EngineFlags { bool thirst=false, sleep=false; bool operator[](int i) const {return i==0?thirst:sleep;} } external;
struct EEngineExternalGame { enum {EnableThirst,EnableSleepiness}; };
EngineFlags& EngineExternal(){return external;}
struct PlayerAddiction { float Current=1, Critical=0, Variability=0, HealthBoost=0, PowerBoost=0; };
struct CArtefact { float m_fHealthRestoreSpeed=0,m_fPowerRestoreSpeed=0,condition=1;
 float m_fBleedingRestoreSpeed=0,m_fSatietyRestoreSpeed=0,m_fThirstRestoreSpeed=0,m_fRadiationRestoreSpeed=0;
 float GetCondition(){return condition;} CArtefact* cast_artefact(){return this;} };
using PIItem=CArtefact*;
struct CCustomOutfit: CArtefact {float m_fPowerLoss=.7f;};
struct Inventory {vector<PIItem> m_belt;};
struct CActorCondition;
struct CActor {bool alive=true,local=true,holder=false; int mstate_real=0; Inventory inv; CCustomOutfit* outfit=nullptr;
 float m_artefact_update_time=0; CActorCondition* cond=nullptr;
 bool g_Alive() const{return alive;} bool Local() const{return local;} void* Holder()const{return holder?(void*)this:nullptr;}
 Inventory& inventory(){return inv;} CCustomOutfit* GetOutfit(){return outfit;}
 CActorCondition& conditions(){return *cond;} void UpdateArtefactsOnBeltAndOutfit();};
struct LevelState { CActor* current=nullptr; void* CurrentViewEntity(){return current;} float GetGameTimeFactor(){return 10;} } level;
LevelState& Level(){return level;}
enum EBoostParams {eBoostHpRestore,eBoostPowerRestore,eBoostBleedingRestore};
struct SBooster {EBoostParams m_type=eBoostHpRestore; float fBoostValue=0, fBoostTime=0;};
using shared_str=string;
struct Sound {bool _feedback(){return false;} void stop(){} void create(string,int,int){} void play(void*,int){}};
enum {st_Effect,sg_SourceType,sm_2D};
struct Settings {bool line_exist(const string&,const char*){return false;} string r_string(const string&,const char*){return "";}} settings;
Settings* pSettings=&settings;
struct UI {void UpdateBoosterIndicators(map<EBoostParams,SBooster>&){}} ui;
struct GameUI {UI* UIMainIngameWnd=&ui;} gameui;
GameUI* CurrentGameUI(){return &gameui;} bool g_dedicated_server=false;
struct CEntityCondition {float m_fDeltaTime=1,m_fDeltaHealth=0,m_fDeltaPower=0,m_fBoostHpRestore=0,m_fBoostPowerRestore=0,m_fBoostBleedingRestore=0;
 struct CV {float m_fV_HealthRestore=.0001f,m_fV_Bleeding=.002f,m_fV_WoundIncarnation=.0003f;} m_change_v;
 bool m_bIsBleeding=false; float bleeding=0;
 bool CanBeHarmed() const{return harmed;} float BleedingSpeed(){return bleeding;} void ChangeBleeding(float){}
 void ChangeHealth(float); void ChangePower(float); void UpdateHealth();};
struct CActorCondition: CEntityCondition {CActor* m_object; PlayerAddiction Satiety,Thirst,Sleepiness; float m_fStandPower=-.001f,m_fPower=1;
 map<EBoostParams,SBooster> m_booster_influences; Sound m_use_sound;
 CActorCondition(CActor& a):m_object(&a){a.cond=this;Thirst.Critical=.2f;Sleepiness.Critical=.5f;Satiety.HealthBoost=.0001f;Satiety.PowerBoost=.005f;level.current=&a;}
 CActor& object() const{return *m_object;}
 ConditionUi::RegenerationSources GetRegenerationSources(bool)const;
 float PowerRestoreEffect(float)const;
 void UpdateSatiety();void UpdateThirst();void UpdateSleepiness();void ConditionStand(float);
 bool ApplyBooster(const SBooster&,const shared_str&,bool);void UpdateBoosters();
 void BoostParameters(const SBooster& b){if(b.m_type==eBoostHpRestore)m_fBoostHpRestore=b.fBoostValue;else if(b.m_type==eBoostPowerRestore)m_fBoostPowerRestore=b.fBoostValue;else m_fBoostBleedingRestore=b.fBoostValue;}
 void DisableBoostParameters(const SBooster& b){SBooster off=b;off.fBoostValue=0;BoostParameters(off);}
 float fdelta_time(){return m_fDeltaTime;} float GetBoostRadiationImmunity(){return 0;}
 void ChangeSatiety(float){} void ChangeThirst(float){} void ChangeRadiation(float){}
};
constexpr float ARTEFACTS_UPDATE_TIME=.1f;
'''
for file,sig,staged in [
 ('src/xrGame/ActorCondition.cpp','float CActorCondition::PowerRestoreEffect',True),
 ('src/xrGame/ActorCondition.cpp','ConditionUi::RegenerationSources CActorCondition::GetRegenerationSources',True),
 ('src/xrGame/EntityCondition.cpp','void CEntityCondition::ChangeHealth',False),
 ('src/xrGame/EntityCondition.cpp','void CEntityCondition::ChangePower',False),
 ('src/xrGame/EntityCondition.cpp','void CEntityCondition::UpdateHealth',False),
 ('src/xrGame/Actor.cpp','void CActor::UpdateArtefactsOnBeltAndOutfit',False),
 *[('src/xrGame/ActorCondition.cpp',f'void CActorCondition::{name}',False) for name in ['UpdateSatiety','UpdateThirst','UpdateSleepiness','ConditionStand','UpdateBoosters']],
 ('src/xrGame/ActorCondition.cpp','bool CActorCondition::ApplyBooster',False)]:
 harness+='\n'+function(file,sig,staged)+'\n'
harness+=r'''
int checks=0;
void equal(float actual,float expected,const char* label){++checks;if(abs(actual-expected)>1e-5f){cerr<<label<<": "<<actual<<" != "<<expected<<endl;exit(1);}}
float hp(CActorCondition& c){return ConditionUi::PercentPerSecond(c.GetRegenerationSources(true).Total(),10);}
float power(CActorCondition& c){return ConditionUi::PercentPerSecond(c.GetRegenerationSources(false).Total(),10);}
int main(int argc,char**){
 external.thirst=external.sleep=argc>1;
 CActor a; CActorCondition c(a); CCustomOutfit outfit; CArtefact af;
 equal(hp(c),.2f,"natural health");equal(power(c),6,"natural + rest");
 a.outfit=&outfit;equal(power(c),6,"unupgraded Sunrise");
 outfit.m_fPowerLoss=1.8f;equal(power(c),6,"power loss cannot affect regeneration");
 a.mstate_real=mcAnyMove;equal(power(c),5,"moving recovery excludes costs");a.mstate_real=0;
 a.holder=true;equal(power(c),5,"holder no standing");a.holder=false;
 for(float satiety:{0.f,.3f,.5f,1.f}){
  c.Satiety.Current=satiety;c.m_fBoostPowerRestore=.005f;
  equal(power(c),10*satiety+1,"energy drink with satiety");
  equal(ConditionUi::PercentPerSecond(c.PowerRestoreEffect(.005f),10),5*satiety,"drink item own effect");
 }
 c.Satiety.Current=1;c.m_fBoostPowerRestore=0;
 af.m_fHealthRestoreSpeed=.0006f;a.inv.m_belt={&af};equal(hp(c),.8f,"Kolobok");
 outfit.m_fHealthRestoreSpeed=.0003f;equal(hp(c),1.1f,"outfit upgrade additive");
 c.ApplyBooster({eBoostHpRestore,.002f,5},"bread",false);equal(hp(c),3.1f,"food active");
 c.ApplyBooster({eBoostHpRestore,.02f,5},"army",false);equal(hp(c),21.1f,"army active");
 c.ApplyBooster({eBoostBleedingRestore,.03f,20},"drug",false);
 c.ApplyBooster({eBoostHpRestore,.01f,10},"medkit",false);equal(hp(c),11.1f,"equal product replaces stronger with longer");
 c.ApplyBooster({eBoostBleedingRestore,.005f,10},"medkit",false);equal(c.m_fBoostBleedingRestore,.03f,"other boost retains stronger product");
 c.ApplyBooster({eBoostHpRestore,.002f,5},"bread",false);equal(hp(c),11.1f,"weaker effect not stacked");
 c.m_fDeltaTime=100;c.UpdateBoosters();equal(hp(c),1.1f,"expired health booster removed");
 c.ApplyBooster({eBoostPowerRestore,.005f,60},"drink",false);equal(power(c),11,"drink active");
 c.m_fDeltaTime=600;c.UpdateBoosters();equal(power(c),6,"expired power booster removed");
 // Compare the new result with unchanged mechanical functions, across source
 // combinations. No damage, resource clamp or movement cost in the comparison.
 for(bool mp:{false,true}) for(bool vulnerable:{false,true}) for(bool standing:{false,true})
 for(float satiety:{0.f,.3f,.5f,1.f}) for(float bonus:{-.003f,0.f,.0006f}) for(float boost:{0.f,.005f}){
  single=!mp;harmed=vulnerable;a.mstate_real=standing?0:mcAnyMove;c.Satiety.Current=satiety;
  af.m_fHealthRestoreSpeed=bonus;af.m_fPowerRestoreSpeed=bonus;af.condition=.7f;
  outfit.m_fHealthRestoreSpeed=.0003f;outfit.m_fPowerRestoreSpeed=.001f;
  c.m_fBoostHpRestore=boost;c.m_fBoostPowerRestore=boost;
  c.m_fDeltaTime=1;c.m_fDeltaHealth=0;c.m_fDeltaPower=0;c.m_fPower=1;
  c.UpdateSatiety();c.UpdateThirst();c.UpdateSleepiness();c.UpdateHealth();
  if(standing)c.ConditionStand(1);
  a.UpdateArtefactsOnBeltAndOutfit();
  equal(c.GetRegenerationSources(true).Total(),c.m_fDeltaHealth,"health vs actual mechanical functions");
  equal(c.GetRegenerationSources(false).Total(),c.m_fDeltaPower+(c.m_fPower-1),"power vs actual mechanical functions");
 }
 single=true;harmed=true;a.mstate_real=0;c.m_fDeltaTime=1;c.m_fBoostHpRestore=0;c.m_fBoostPowerRestore=0;
 // Optional body mechanics stay off; when enabled, the same gates/formulas apply.
 c.Thirst={.3f,.2f,0,.002f,.003f};c.Sleepiness={.8f,.3f,0,.003f,.004f};
 c.m_fDeltaHealth=c.m_fDeltaPower=0;c.m_fPower=1;
 c.UpdateSatiety();c.UpdateThirst();c.UpdateSleepiness();c.UpdateHealth();c.ConditionStand(1);a.UpdateArtefactsOnBeltAndOutfit();
 equal(c.GetRegenerationSources(true).Total(),c.m_fDeltaHealth,"optional health mechanics");
 equal(c.GetRegenerationSources(false).Total(),c.m_fDeltaPower+c.m_fPower-1,"optional power mechanics");
 external.thirst=external.sleep=false;
 const float previous=power(c);c.m_fPower=1;equal(power(c),previous,"full power keeps recovery");
 c.m_fPower=.1f;equal(power(c),previous,"low power same source rate");
 a.alive=false;equal(power(c),0,"dead actor");equal(hp(c),0,"dead actor health");a.alive=true;
 god=true;equal(power(c),0,"god mode");god=false;
 c.Satiety.Current=1;a.inv.m_belt.clear();a.outfit=nullptr;
 for(float factor:{.5f,1.f,5.f,10.f,20.f}){
  equal(ConditionUi::PercentPerSecond(c.GetRegenerationSources(false).Total(),factor),.6f*factor,"dynamic time");
 }
 for(float value:{-.01f,0.f,2.5f,2.5001f,10.2f}){
  float fill=value/ConditionUi::HealthRegenerationMaximum;clamp(fill,0.f,1.f);
  equal(fill,max(0.f,min(1.f,value/2.5f)),"health bar range");
  ++checks;assert((value>ConditionUi::HealthRegenerationMaximum)==(value>2.5f));
 }
 char number[32];
 ConditionUi::FormatNumber(number,.8,',',true);++checks;assert(string(number)=="+0,8");
 ConditionUi::FormatNumber(number,-.002,',',true);++checks;assert(string(number)=="-<0,01");
 ConditionUi::FormatNumber(number,.002,',',true);++checks;assert(string(number)=="+<0,01");
 ConditionUi::FormatNumber(number,0,',',true);++checks;assert(string(number)=="0");
 cout<<checks<<" compiled checks passed (source functions extracted verbatim)"<<endl;
}
'''
temporary=tempfile.TemporaryDirectory(prefix='ixray-regeneration-test-')
out=Path(temporary.name)
(out/'regeneration.cpp').write_text(harness)
subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Wno-unused-parameter','-I'+str(S/'src/xrGame'),str(out/'regeneration.cpp'),'-o',str(out/'regeneration')],check=True)
subprocess.run([str(out/'regeneration')],check=True)
subprocess.run([str(out/'regeneration'),'optional-mechanics'],check=True)

M=args.mod_root
ids=None
for lang in ['pol','eng','cze','rus']:
 p=M/f'configs/text/{lang}/zz_uiparams_panel.xml'
 text=p.read_bytes().decode('cp1250' if lang in ['pol','cze'] else 'cp1251')
 tree=ET.fromstring(re.sub(r'<\?xml[^>]+>','',text))
 names=[x.attrib['id'] for x in tree.findall('string')]
 assert len(names)==len(set(names)),lang
 if ids is None:ids=set(names)
 else:assert set(names)==ids,lang
 assert 'x1000' not in ''.join(''.join(x.itertext()) for x in tree if x.attrib['id'].startswith('ui_uip_tt_reg_'))
for p in (M/'configs/ui').rglob('*.xml'):ET.parse(p)
panel=ET.parse(M/'configs/ui/mod_actor_menu_16_uiparams.xml')
for tag in ['thirst_state','power_sensor']:
 row=panel.find('.//'+tag);assert row.attrib['regeneration']=='1'
 assert 'expression' not in row.find('state_progress').attrib
 assert row.find('value') is not None and row.find('overflow') is not None
print('XML valid,',len(ids),'identical translation IDs in PL/EN/CZ/RU; atomic panel wiring checked')
