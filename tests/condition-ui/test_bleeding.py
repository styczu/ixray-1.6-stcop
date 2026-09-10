"""Exercise production bleeding UI, restoration aggregation and wound healing."""
from pathlib import Path
import argparse, subprocess, tempfile, re, xml.etree.ElementTree as ET
p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--mod-root',type=Path,required=True);p.add_argument('--base-root',type=Path);a=p.parse_args()
E=a.source_root; B=a.base_root or E

def func(path,sig):
 f=E/path
 if not f.exists():f=B/path
 s=f.read_bytes().decode('latin1').replace('\r\n','\n');i=s.index(sig); j=s.index('{',i); depth=1;k=j+1
 while depth:depth+=(s[k]=='{')-(s[k]=='}');k+=1
 return s[i:k]
xml=ET.parse(a.mod_root/'configs/ui/mod_actor_menu_16_uiparams.xml');row=xml.find('.//bleeding_state');assert row.get('bleeding')=='1'
assert row.find('value/text').get('font')=='ui_font_panel'
assert not row.find('state_progress').get('expression')
assert not any('BleedingSpeed' in n.get('expression','') for n in xml.iter())
assert float(row.find('state_progress').get('x'))+float(row.find('state_progress').get('width')) < float(row.find('overflow').get('x'))
# Five digits (the wound-component cap can produce values above 1000) fit alongside the triangle.
langs={lang:{x.get('id'):x.findtext('text') for x in ET.parse(a.mod_root/f'configs/text/{lang}/zz_uiparams_panel.xml').findall('string')} for lang in ['pol','eng','cze','rus']}
assert all(set(v)==set(langs['pol']) for v in langs.values())
PL=langs['pol'];assert PL['ui_uip_tt_bleed_level']=='Intensywność:'

def cstr(v):
 import json
 return json.dumps(v,ensure_ascii=False)
strings='std::map<std::string,std::string> strings={'+','.join('{'+cstr(k)+','+cstr(v)+'}' for k,v in PL.items())+'};'
source=r'''
#include "ConditionUiValues.h"
#include <algorithm>
#include <cassert>
#include <map>
#include <string>
#include <vector>
#include <iostream>
using LPCSTR=const char*;using string32=char[32];using xr_string=std::string;using shared_str=std::string;
#define xr_sprintf std::snprintf
#define VERIFY assert
bool _valid(float x){return std::isfinite(x);}bool fis_zero(float x){return std::fabs(x)<1e-6;}
'''+strings+r'''
struct Table {std::string translate(const char* s){return strings.at(s);}} table;Table* g_pStringTable=&table;
float tf=10;bool god=false;bool GodMode(){return god;}
namespace ConditionUi {float CurrentTimeFactor(){return tf;}char DecimalSeparator(){return ',';}}
static LPCSTR kBreak="\\n", kColTitle="%c[255,224,230,234]",kColLabel="%c[255,176,182,186]",kColSep="%c[255,96,104,108]";
struct Progress{float pos=0;void SetProgressPosImmediate(float v){pos=std::clamp(v,0.f,1.f);}};
struct Static{bool shown=false;void Show(bool v){shown=v;}};
struct ui_actor_state_item {std::string text,hint;Progress progress;Static frame,fill;
 Progress* m_progress=&progress;Static* m_overflow=&frame,*m_overflow_fill=&fill;
 void set_value_text(const char* s){text=s;}void set_hint_text(const char*s){hint=s;}
 void set_bleeding(float);};
struct CArtefact{float m_fBleedingRestoreSpeed=0,condition=1;float GetCondition(){return condition;}CArtefact* cast_artefact(){return this;}};
struct CCustomOutfit{float m_fBleedingRestoreSpeed=0;};using PIItem=CArtefact*;
struct Condition{float bleeding=1.21f,boost=0;bool harmed=true;struct CV{float m_fV_Bleeding=.002f,m_fV_WoundIncarnation=.0003f;} cv;
 float BleedingSpeed(){return bleeding;}bool CanBeHarmed(){return harmed;}CV& change_v(){return cv;}float GetBoostBleedingRestore(){return boost;}};
namespace ALife{enum{eBleedingRestoreSpeed,eHitTypeMax=3};}
struct CActor{Condition c;CCustomOutfit* outfit=nullptr;struct Inv{std::vector<PIItem> m_belt;} inv;
 Inv& inventory(){return inv;}Condition& conditions(){return c;}CCustomOutfit* GetOutfit(){return outfit;}float GetRestoreSpeed(int);};
struct ui_actor_state_wnd{enum{stt_bleeding=0};ui_actor_state_item item;ui_actor_state_item* m_state[1]={&item};void UpdateBleedingInfo(CActor*);};
struct CWound {std::vector<float> m_Wounds=std::vector<float>(3);float TotalSize();void Incarnation(float,float);};
struct CEntityCondition {std::vector<CWound*> m_WoundVector;float BleedingSpeed();};
using WOUND_VECTOR_IT=std::vector<CWound*>::iterator;
'''
for path,sig in [('src/xrGame/ui/UIActorStateInfo.cpp','void FormatRate'),('src/xrGame/ui/UIActorStateInfo.cpp','void ui_actor_state_item::set_bleeding'),('src/xrGame/ui/UIActorStateInfo.cpp','void ui_actor_state_wnd::UpdateBleedingInfo'),('src/xrGame/Wound.cpp','float CWound::TotalSize'),('src/xrGame/Wound.cpp','void CWound::Incarnation'),('src/xrGame/EntityCondition.cpp','float CEntityCondition::BleedingSpeed')]:source+='\n'+func(path,sig)
restore=func('src/xrGame/Actor.cpp','float CActor::GetRestoreSpeed');case=restore[restore.index('case ALife::eBleedingRestoreSpeed:'):restore.index('}//switch')]
source+='\nfloat CActor::GetRestoreSpeed(int type){float res=0;switch(type){'+case+'}return res;}\n'
source+=r'''
std::string plain(std::string s){for(size_t p;(p=s.find("%c["))!=std::string::npos;)s.erase(p,s.find(']',p)-p+1);return s;}
int main(){
 ui_actor_state_wnd ui;CActor actor;
 const struct{float raw;const char* number;bool overflow;} cases[]={{0,"0",false},{.0000001f,"1",false},{.995f,"100",false},{1,"100",false},{1.00001f,"101",true},{1.21f,"121",true},{1.21001f,"122",true},{110,"11000",true}};
 for(auto c:cases){actor.c.bleeding=c.raw;ui.UpdateBleedingInfo(&actor);assert(ui.item.text==c.number);assert(ui.item.frame.shown==c.overflow);assert(ui.item.fill.shown==c.overflow);assert(std::fabs(ui.item.progress.pos-std::clamp(c.raw,0.f,1.f))<1e-6);}
 actor.c.bleeding=1.21f;ui.UpdateBleedingInfo(&actor);auto hint=plain(ui.item.hint);
 assert(hint.find("Intensywność: 121,00")!=std::string::npos);
 assert(hint.find("Ubytek zdrowia: 2,42%/s")!=std::string::npos);
 assert(hint.find("Redukcja krwawienia: 0,3/s")!=std::string::npos);
 assert(hint.find("Intensywność:")<hint.find("Ubytek zdrowia:"));assert(hint.find("Redukcja krwawienia:")<hint.find("Powoduje"));
 CArtefact af;af.m_fBleedingRestoreSpeed=.001f;af.condition=.5f;actor.inv.m_belt.push_back(&af);
 CCustomOutfit outfit;outfit.m_fBleedingRestoreSpeed=.0002f;actor.outfit=&outfit;actor.c.boost=.02f;
 ui.UpdateBleedingInfo(&actor);assert(plain(ui.item.hint).find("Redukcja krwawienia: 21/s")!=std::string::npos);
 actor.inv.m_belt.clear();actor.outfit=nullptr;actor.c.boost=0;tf=5;ui.UpdateBleedingInfo(&actor);
 assert(plain(ui.item.hint).find("Ubytek zdrowia: 1,21%/s")!=std::string::npos);
 assert(plain(ui.item.hint).find("Redukcja krwawienia: 0,15/s")!=std::string::npos);
 actor.c.harmed=false;ui.UpdateBleedingInfo(&actor);assert(plain(ui.item.hint).find("Ubytek zdrowia: 0%/s")!=std::string::npos);
 actor.c.harmed=true;god=true;ui.UpdateBleedingInfo(&actor);assert(plain(ui.item.hint).find("Ubytek zdrowia: 0%/s")!=std::string::npos);god=false;
 ui.item.m_progress=nullptr;ui.item.m_overflow=nullptr;ui.item.m_overflow_fill=nullptr;ui.UpdateBleedingInfo(&actor);
 // Actual wound mechanics: the same 0.3/s strength can reduce two components by 0.6/s.
 CWound w;w.m_Wounds={.6f,.61f,0};CEntityCondition condition;condition.m_WoundVector={&w};
 assert(std::fabs(ConditionUi::BleedingIntensity(condition.BleedingSpeed())-121)<1e-4);
 w.Incarnation(.003f,0);assert(std::fabs(ConditionUi::BleedingIntensity(condition.BleedingSpeed())-120.4f)<1e-4);
 CWound other;other.m_Wounds={.4f,0,0};condition.m_WoundVector.push_back(&other);
 assert(std::fabs(condition.BleedingSpeed()-(1.204f+.4f)/2)<1e-5);
 std::cout<<hint<<"\nPASS: panel boundaries, tooltip, equipment condition, medicine, time factors, invulnerability and real wound mechanics\n";
}
'''
with tempfile.TemporaryDirectory() as d:
 f=Path(d)/'test.cpp';f.write_text(source);out=Path(d)/'test'
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I',str(E/'src/xrGame'),str(f),'-o',str(out)],check=True)
 subprocess.run([str(out)],check=True)
print('PASS: XML wiring and language IDs')
