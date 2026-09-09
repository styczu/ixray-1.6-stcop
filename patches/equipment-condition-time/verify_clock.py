#!/usr/bin/env python3
"""Test naliczania czasu na funkcji odczytanej ze wskazanych źródeł IX-Ray."""
from pathlib import Path
import argparse
import subprocess
import tempfile

PREFIX = r'''
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <cstdlib>
struct CArtefact;
struct Item {virtual ~Item()=default;virtual CArtefact* cast_artefact(){return nullptr;}};
using PIItem=Item*;
struct Rates {
 float m_fBleedingRestoreSpeed=.003f,m_fHealthRestoreSpeed=.0002f,
 m_fPowerRestoreSpeed=.001f,m_fSatietyRestoreSpeed=.0001f,
 m_fThirstRestoreSpeed=.0003f,m_fRadiationRestoreSpeed=.002f;
};
struct CArtefact:Item,Rates {
 float condition=1;float GetCondition(){return condition;}
 CArtefact* cast_artefact() override{return this;}
};
struct CCustomOutfit:Rates{};
struct Conditions {
 float dt=0,immunity=0;
 double bleeding=0,health=0,power=0,satiety=0,thirst=0,radiation=0;
 float fdelta_time(){return dt;}
 float GetBoostRadiationImmunity(){return immunity;}
 void ChangeBleeding(float v){bleeding+=v;}
 void ChangeHealth(float v){health+=v;}
 void ChangePower(float v){power+=v;}
 void ChangeSatiety(float v){satiety+=v;}
 void ChangeThirst(float v){thirst+=v;}
 void ChangeRadiation(float v){radiation+=v;}
};
struct Inventory{std::vector<PIItem> m_belt;};
struct CActor {
 Conditions state;Inventory inv;CCustomOutfit* outfit=nullptr;
 float m_artefact_update_time=0;
 Conditions& conditions(){return state;}
 Inventory& inventory(){return inv;}
 CCustomOutfit* GetOutfit(){return outfit;}
 void UpdateArtefactsOnBeltAndOutfit();
};
template<class T> T _max(T a,T b){return std::max(a,b);}
template<class T> void clamp(T& x,T lo,T hi){x=std::max(lo,std::min(x,hi));}
#define ARTEFACTS_UPDATE_TIME 0.100f
'''

TEST = r'''
int checks=0;
void near(double got,double expected){
 if(std::abs(got-expected)>.00001){
  std::fprintf(stderr,"FAIL: naliczono %.9f, oczekiwano %.9f\n",got,expected);std::exit(1);
 }
 ++checks;
}
int main(){
 for(float interval:{.005f,.01666667f,.03333333f,.1f,.25f}){
  for(float factor:{1.f,10.f,20.f}){
   CActor a;CArtefact art;CCustomOutfit suit;
   art.condition=.6f;a.inv.m_belt={&art};a.outfit=&suit;
   double elapsed=0;
   for(int i=0;i<600;++i){
    float dt=i%13==0?0.f:interval*factor*(i%7==0?1.3f:1.f);
    elapsed+=dt;a.state.dt=dt;a.UpdateArtefactsOnBeltAndOutfit();
   }
   double applied=elapsed-a.m_artefact_update_time;
   near(a.state.health,applied*(art.m_fHealthRestoreSpeed*.6+suit.m_fHealthRestoreSpeed));
   near(a.state.power,applied*(art.m_fPowerRestoreSpeed*.6+suit.m_fPowerRestoreSpeed));
   near(a.state.bleeding,applied*(art.m_fBleedingRestoreSpeed*.6+suit.m_fBleedingRestoreSpeed));
   near(a.state.satiety,applied*(art.m_fSatietyRestoreSpeed*.6+suit.m_fSatietyRestoreSpeed));
   near(a.state.thirst,applied*(art.m_fThirstRestoreSpeed*.6+suit.m_fThirstRestoreSpeed));
   near(a.state.radiation,applied*(art.m_fRadiationRestoreSpeed*.6+suit.m_fRadiationRestoreSpeed));
   if(a.m_artefact_update_time<0 || a.m_artefact_update_time>=.1f)return 1;
   ++checks;
  }
 }
 CActor a,b;CArtefact x,y;a.inv.m_belt={&x};b.inv.m_belt={&y};
 a.state.dt=.04f;a.UpdateArtefactsOnBeltAndOutfit();
 b.state.dt=.07f;b.UpdateArtefactsOnBeltAndOutfit();
 near(a.state.health,0);near(b.state.health,0);
 a.state.dt=0;a.UpdateArtefactsOnBeltAndOutfit();near(a.m_artefact_update_time,.04f);
 a.state.dt=.061f;a.UpdateArtefactsOnBeltAndOutfit();near(a.state.health,.101*.0002);
 near(b.m_artefact_update_time,.07f);
 CActor c;CArtefact emitter,absorber;absorber.m_fRadiationRestoreSpeed=-.002f;
 c.inv.m_belt={&emitter,&absorber};c.state.dt=.2f;
 c.UpdateArtefactsOnBeltAndOutfit();near(c.state.radiation,0);
 c.state.immunity=.001f;c.UpdateArtefactsOnBeltAndOutfit();near(c.state.radiation,-.0002);
 std::printf("OK: %d sprawdzen czasu, pauz, oddzielnych licznikow i efektow sprzetu.\n",checks);
}
'''


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 0
    for pos in range(opening, len(source)):
        depth += (source[pos] == '{') - (source[pos] == '}')
        if depth == 0:
            return source[start:pos+1]
    raise ValueError('Nie znaleziono końca funkcji: ' + signature)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path.cwd())
    parser.add_argument('--cxx', default='g++')
    args = parser.parse_args()
    source = (args.repo/'src/xrGame/Actor.cpp').read_bytes().decode('utf-8',errors='surrogateescape')
    source = source.replace('\r\n','\n')
    code = function(source,'void CActor::UpdateArtefactsOnBeltAndOutfit()')
    with tempfile.TemporaryDirectory(prefix='ixray-clock-test-') as directory:
        directory=Path(directory)
        cpp=directory/'clock.cpp';binary=directory/'clock-test'
        cpp.write_text(PREFIX+'\n'+code+'\n'+TEST,errors='surrogateescape')
        subprocess.run([args.cxx,'-std=c++17','-Wall','-Wextra','-Werror','-pedantic',str(cpp),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
    # The body test also runs against old sources (and fails for lost time).
    # These checks cover the new counter's integration and lifecycle reset.
    header=(args.repo/'src/xrGame/Actor.h').read_text(errors='surrogateescape')
    network=(args.repo/'src/xrGame/Actor_Network.cpp').read_text(errors='surrogateescape')
    assert 'float m_artefact_update_time = 0.0f;' in header
    assert 'm_artefact_update_time = 0.0f;' in function(source,'void CActor::reinit')
    assert 'm_artefact_update_time = 0.0f;' in function(network,'void CActor::load')
    print('OK: inicjalizacja licznika oraz reset przy reinicjalizacji i wczytaniu.')
    print('To test wybranej funkcji, nie pełna kompilacja ani próba w grze.')


if __name__ == '__main__':
    main()
