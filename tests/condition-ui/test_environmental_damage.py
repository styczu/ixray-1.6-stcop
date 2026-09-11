"""Compile the production hit calculation, observer and history with small fixtures.

The outfit boundary is a fixture; test_protection.py separately exercises the
production outfit, helmet and artefact calculations. No Windows renderer here.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
def read(path):
    return (root / path).read_bytes().decode('latin1').replace('\r\n', '\n')
def function(path, signature):
    text = read(path)
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
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
#include <cmath>
#include <cstdint>
#include <cstdio>
using LPCSTR=const char*;using u32=uint32_t;
'''
cpp += 'namespace ALife {\n'+enums+'\n}\n#define XRAY_ALIFE_SPACE\n#include "EnvironmentalDamage.h"\n'
cpp += r'''
struct {uint32_t dwTimeGlobal=0;} Device;
struct CWound {} wound;
struct Object {int ID(){return 1;}const char*Name(){return "actor";}void*Visual(){return nullptr;}} object;
struct Kinematics {const char*LL_BoneName_dbg(int){return "bone";}} kinematics;
Kinematics*PKinematics(void*){return &kinematics;}
void Msg(const char*,...){}
#define R_ASSERT2(x,...) assert(x)
constexpr int BI_NONE=-1;bool bDebug=false,god=false;
bool GodMode(){return god;}
struct SHit {Object*who=&object;int boneID=0;bool add_wound=false;float power=0,armor_piercing=0;ALife::EHitType hit_type=ALife::eHitTypeBurn;float damage(){return power;}};
struct CEntityCondition {
 Object*m_object=&object,*m_pWho=nullptr;int m_iWhoID=0;
 float m_fBoostTelepaticProtection=0,m_fBoostTelepaticImmunity=0,m_fBoostBurnImmunity=0;
 float m_fBoostChemicalBurnProtection=0,m_fBoostChemicalBurnImmunity=0,m_fBoostShockImmunity=0;
 float m_fBoostRadiationProtection=0,m_fBoostRadiationImmunity=0,m_fBoostExplImmunity=0;
 float m_fBoostStrikeImmunity=0,m_fBoostFireWoundImmunity=0,m_fBoostWoundImmunity=0;
 float m_fHealthLost=0,m_fHealthHitPart=.5f,m_fPowerHitPart=.2f,m_fHitBoneScale=.8f,m_fWoundBoneScale=1;
 float m_fDeltaHealth=0,m_fDeltaPsyHealth=0,m_fDeltaRadiation=0,m_fDeltaPower=0;
 float protection=.1f,immunity=.75f;bool harmed=true;
 float GetHitImmunity(ALife::EHitType){return immunity;}
 float HitOutfitEffect(float power,ALife::EHitType,int,float,bool&){return std::max(0.f,power-protection);}
 void ChangePsyHealth(float amount){m_fDeltaPsyHealth+=amount;}
 bool CanBeHarmed(){return harmed;}float GetHealth(){return 1;}
 CWound*AddWound(float,ALife::EHitType,int){return &wound;}
 CWound*ConditionHit(SHit*);
};
struct CActorCondition:CEntityCondition {
 using inherited=CEntityCondition;
 Protection::DamageHistory m_environmental_damage;
 CWound*ConditionHit(SHit*);
 Protection::DamageReading GetEnvironmentalDamage(ALife::EHitType)const;
};
'''
cpp += function('src/xrGame/EntityCondition.cpp','CWound* CEntityCondition::ConditionHit')
cpp += function('src/xrGame/ActorCondition.cpp','CWound* CActorCondition::ConditionHit')
cpp += function('src/xrGame/ActorCondition.cpp','Protection::DamageReading CActorCondition::GetEnvironmentalDamage')
cpp += r'''
unsigned checks=0;
void eq(float a,float b){++checks;assert(std::fabs(a-b)<1e-6f);}
int main(){
 const auto burn=ALife::eHitTypeBurn,chem=ALife::eHitTypeChemicalBurn,shock=ALife::eHitTypeShock;
 const auto psi=ALife::eHitTypeTelepatic,rad=ALife::eHitTypeRadiation;
 SHit hit;hit.power=.5f;CActorCondition actor;
 // Existing healing and unrelated damage in accumulators must cancel out.
 actor.m_fDeltaHealth=.31f;actor.m_fDeltaPsyHealth=.12f;actor.m_fDeltaRadiation=-.03f;
 actor.m_fBoostBurnImmunity=.25f;
 auto resolved=actor.ConditionHit(&hit);assert(resolved==nullptr);
 auto sample=actor.GetEnvironmentalDamage(burn);assert(sample.valid);
 eq(sample.health,.08f);eq(sample.psy,0);eq(sample.radiation,0);
 eq(actor.m_fDeltaHealth,.23f); // Real calculation was neither replaced nor applied twice.
 // Actual type-specific medication subtraction, immunity and hit-part factors.
 hit.hit_type=chem;actor.m_fBoostChemicalBurnProtection=.2f;actor.m_fBoostChemicalBurnImmunity=.25f;
 actor.ConditionHit(&hit);sample=actor.GetEnvironmentalDamage(chem);eq(sample.health,.05f);
 hit.hit_type=shock;actor.m_fBoostShockImmunity=.5f;
 actor.ConditionHit(&hit);eq(actor.GetEnvironmentalDamage(shock).health,.05f);
 hit.hit_type=psi;actor.m_fBoostTelepaticProtection=.1f;actor.m_fBoostTelepaticImmunity=.25f;
 actor.ConditionHit(&hit);sample=actor.GetEnvironmentalDamage(psi);
 eq(sample.health,.075f);eq(sample.psy,.15f);eq(sample.radiation,0);
 hit.hit_type=rad;actor.m_fBoostRadiationProtection=.2f;actor.m_fBoostRadiationImmunity=.25f;
 actor.ConditionHit(&hit);sample=actor.GetEnvironmentalDamage(rad);
 eq(sample.radiation,.1f);eq(sample.health,0);eq(sample.psy,0);
 // Zero replaces previous loss. Light burn is the same UI channel.
 hit.hit_type=ALife::eHitTypeLightBurn;hit.power=.05f;actor.ConditionHit(&hit);
 assert(actor.GetEnvironmentalDamage(burn).valid);eq(actor.GetEnvironmentalDamage(burn).health,0);
 eq(actor.GetEnvironmentalDamage(ALife::eHitTypeLightBurn).health,0);
 // God mode clears a previously damaging channel and leaves accumulators alone.
 hit.hit_type=chem;god=true;float before=actor.m_fDeltaHealth;actor.ConditionHit(&hit);
 eq(actor.GetEnvironmentalDamage(chem).health,0);eq(actor.m_fDeltaHealth,before);god=false;
 actor.harmed=false;hit.power=.5f;hit.hit_type=shock;actor.ConditionHit(&hit);
 eq(actor.GetEnvironmentalDamage(shock).health,0);
 // Overpowered immunity can heal: do not claim it as negative damage.
 actor.harmed=true;actor.m_fBoostBurnImmunity=2;hit.hit_type=burn;actor.ConditionHit(&hit);
 eq(actor.GetEnvironmentalDamage(burn).health,0);
 // Non-environmental hits preserve their wound return and are never shown.
 hit.hit_type=ALife::eHitTypeWound;hit.add_wound=true;
 assert(actor.ConditionHit(&hit)==&wound);assert(!actor.GetEnvironmentalDamage(hit.hit_type).valid);
 // Use the real clock getter, with strict expiry and no advance during pause.
 Device.dwTimeGlobal=2999;assert(actor.GetEnvironmentalDamage(burn).valid);
 for(int i=0;i<100;++i)assert(actor.GetEnvironmentalDamage(burn).valid);
 Device.dwTimeGlobal=3000;assert(!actor.GetEnvironmentalDamage(burn).valid);
 Protection::DamageHistory history;
 assert(!history.Get(burn,0).valid);
 history.Record(burn,.08f,.02f,.03f,100);
 sample=history.Get(burn,100);assert(sample.valid);eq(sample.health,.08f);eq(sample.psy,.02f);eq(sample.radiation,.03f);
 assert(!history.Get(shock,100).valid);
 history.Record(burn,0,0,0,200);assert(history.Get(burn,200).valid);eq(history.Get(burn,200).health,0);
 history.Record(burn,-1,-2,-3,201);sample=history.Get(burn,201);eq(sample.health,0);eq(sample.psy,0);eq(sample.radiation,0);
 for(auto type:{ALife::eHitTypeMax,ALife::eHitTypeExplosion}){
  history.Record(type,1,1,1,201);assert(!history.Get(type,201).valid);
 }
 history.Reset();history.Record(burn,NAN,0,0,1);history.Record(burn,0,INFINITY,0,1);history.Record(burn,0,0,NAN,1);
 assert(!history.Get(burn,1).valid);
 history.Record(burn,1,0,0,1000);assert(!history.Get(burn,10).valid);
 history.Reset();history.Record(burn,1,0,0,UINT32_MAX-100);assert(history.Get(burn,100).valid);
 history.Reset();assert(!history.Get(burn,100).valid);
 std::printf("%u production damage checks plus history/expiry/zero/god-mode assertions passed\n",checks);
}
'''
with tempfile.TemporaryDirectory(prefix='environmental-damage-') as tmp:
    work=Path(tmp)
    (work/'test.cpp').write_text(cpp)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer','-fno-sanitize-recover=all','-no-pie','-I'+str(root/'src/xrGame'),
                    str(work/'test.cpp'),'-o',str(work/'test')],check=True)
    subprocess.run([str(work/'test')],check=True)
for signature in ['void CActorCondition::load(', 'void CActorCondition::reinit()']:
    assert 'm_environmental_damage.Reset();' in function('src/xrGame/ActorCondition.cpp',signature)
print('Damage history reset on load/reinit passed')
