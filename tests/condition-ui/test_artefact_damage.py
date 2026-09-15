"""Check production artefact/outfit/helmet calculations independently of UI fixtures.

Item durability and engine services are fixtures; no running-game measurement.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]

def read(path):
    return (root / path).read_bytes().decode('latin1').replace('\r\n', '\n')

def function(path, signature):
    text = read('src/xrGame/' + path)
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end] + '\n'

cpp = r'''
#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <iostream>
#include <vector>
using u32=unsigned int;using s16=short;using std::pow;
#define VERIFY(x) assert(x)
#define flt_max FLT_MAX
template<class T>void clamp(T&v,T lo,T hi){v=std::max(lo,std::min(v,hi));}
'''
cpp += 'namespace ALife {\n' + re.search(
    r'enum EHitType\s*\{.*?\};', read('src/xrEngine/AI/alife_space.h'), re.S)[0] + '\n}\n'
cpp += r'''
struct CArtefact;
struct Item {float condition=1;float GetCondition(){return condition;}virtual CArtefact*cast_artefact(){return nullptr;}};
struct Immunities {float v[ALife::eHitTypeMax]={};float AffectHit(float p,ALife::EHitType t){return p*v[t];}};
struct CArtefact:Item {Immunities m_ArtefactHitImmunities;CArtefact*cast_artefact()override{return this;}};
using PIItem=Item*;
struct Inventory{std::vector<PIItem>m_belt;};
struct CActor {Inventory inv;Inventory&inventory(){return inv;}
 float HitArtefactsOnBelt(float,ALife::EHitType);float GetProtection_ArtefactsOnBelt(ALife::EHitType);};
struct Bones {float m_fHitFracActor=.1f;float getBoneProtection(s16){return 1;}} bones;
bool IsGameTypeSingle(){return true;}
struct CCustomOutfit:Item {float m_HitTypeProtection[ALife::eHitTypeMax]={};Bones*m_boneProtection=&bones;
 float GetDefHitTypeProtection(ALife::EHitType);float HitThroughArmor(float,s16,float,bool&,ALife::EHitType);
 float GetBoneArmor(s16){return .2f;}void Hit(float,ALife::EHitType){} };
struct CHelmet:CCustomOutfit {float GetDefHitTypeProtection(ALife::EHitType);float HitThroughArmor(float,s16,float,bool&,ALife::EHitType);};
'''
for file, signature in [
    ('Actor.cpp', 'float CActor::HitArtefactsOnBelt'),
    ('Actor.cpp', 'float CActor::GetProtection_ArtefactsOnBelt'),
    ('CustomOutfit.cpp', 'float CCustomOutfit::GetDefHitTypeProtection'),
    ('CustomOutfit.cpp', 'float CCustomOutfit::HitThroughArmor'),
    ('ActorHelmet.cpp', 'float CHelmet::GetDefHitTypeProtection'),
    ('ActorHelmet.cpp', 'float CHelmet::HitThroughArmor'),
]:
    cpp += function(file, signature)
cpp += r'''
int checks=0;
void eq(double got,double want){++checks;if(std::abs(got-want)>1e-6+std::abs(want)*1e-5){std::cerr<<got<<" != "<<want<<"\n";std::abort();}}
double expected(double hit,double sum){
 if(sum==0)return hit;
 double reduction=1.5*std::exp(4*std::log(.9)/std::min(std::abs(sum),double(.99f)));
 return hit*(1+(sum<0?reduction:-reduction));
}
int main(){
 CActor actor;CArtefact a,b;Item unrelated;CCustomOutfit outfit;CHelmet helmet;
 const auto types={ALife::eHitTypeBurn,ALife::eHitTypeLightBurn,ALife::eHitTypeShock,
  ALife::eHitTypeChemicalBurn,ALife::eHitTypeRadiation,ALife::eHitTypeTelepatic};
 for(auto type:types){
  actor.inv.m_belt={&a,&unrelated,&b};
  for(float ac:{0.f,.01f,.5f,1.f})for(float bc:{0.f,.5f,1.f})
  for(float av:{-2.f,-.1f,0.f,.02f,.04f,.1f,.99f,2.f})for(float bv:{-.1f,0.f,.1f}){
   a.condition=ac;b.condition=bc;a.m_ArtefactHitImmunities.v[type]=av;b.m_ArtefactHitImmunities.v[type]=bv;
   const float sum=av*ac+bv*bc;eq(actor.GetProtection_ArtefactsOnBelt(type),sum);
   for(float hit:{0.f,.001f,.02f,.2f,1.f,3.f}){
    float after=actor.HitArtefactsOnBelt(hit,type);eq(after,expected(hit,sum));
    for(float condition:{0.f,.5f,1.f}){
     outfit.condition=condition;helmet.condition=condition;
     outfit.m_HitTypeProtection[type]=.3f;helmet.m_HitTypeProtection[type]=.2f;
     bool wound=true;float remaining=outfit.HitThroughArmor(after,0,0,wound,type);
     remaining=helmet.HitThroughArmor(remaining,0,0,wound,type);
     eq(remaining,std::max(0.0,expected(hit,sum)-.05*condition));
    }
   }
  }
  actor.inv.m_belt={&a};a.condition=1;a.m_ArtefactHitImmunities.v[type]=.1f;
  eq(actor.HitArtefactsOnBelt(1,type),.977828695);
  a.m_ArtefactHitImmunities.v[type]=-.1f;eq(actor.HitArtefactsOnBelt(1,type),1.022171305);
  a.m_ArtefactHitImmunities.v[type]=.1f;b.m_ArtefactHitImmunities.v[type]=.1f;b.condition=1;
  actor.inv.m_belt={&a,&b};eq(actor.HitArtefactsOnBelt(1,type),.817635018);
 }
 actor.inv.m_belt.clear();eq(actor.HitArtefactsOnBelt(.2f,ALife::eHitTypeBurn),.2f);
 std::cout<<checks<<" production artefact/armour checks passed\n";
}
'''
with tempfile.TemporaryDirectory(prefix='ixray-artefact-damage-') as tmp:
    work = Path(tmp)
    (work / 'test.cpp').write_text(cpp)
    subprocess.run(['g++', '-std=c++17', '-O1', '-g', '-fsanitize=address,undefined',
                    str(work / 'test.cpp'), '-o', str(work / 'test')], check=True)
    subprocess.run([str(work / 'test')], check=True)

hit = function('Actor.cpp', 'void\tCActor::Hit(SHit* pHDS)')
single_player = hit[hit.index('float hit_power = HitArtefactsOnBelt'):]
assert single_player.index('HitArtefactsOnBelt') < single_player.index('inherited::Hit(&HDS)')
condition_hit = function('EntityCondition.cpp', 'CWound* CEntityCondition::ConditionHit')
assert condition_hit.index('HitOutfitEffect(') < condition_hit.index('switch(pHDS->hit_type)')
armour = function('EntityCondition.cpp', 'float CEntityCondition::HitOutfitEffect')
assert armour.index('pOutfit->HitThroughArmor') < armour.index('pHelmet->HitThroughArmor')
print('Production wiring: artefacts, outfit, helmet, then actor condition modifiers passed')
