#include "TrackingTools/DetLayers/interface/GeomDetCompatibilityChecker.h"
#include "TrackingTools/TrajectoryState/interface/TrajectoryStateOnSurface.h" 

#include "TrackingTools/GeomPropagators/interface/StraightLinePlaneCrossing.h"


#include<iostream>

namespace{

  struct Stat {

    long long ntot=0;
    long long nf1=0;
    long long nf2=0;

    long long ns1=0;
    long long ns2=0;
    long long ns11=0;
    long long ns21=0;

    long long nth=0;
    long long nle=0;


                            //Geom checker     1337311   84696       634946      20369      259701        241266       18435         614128
                            //    Geom checker 124,567,704  3,862,821 36,055,605 3,799,127 29,825,229    4,573,316     320,063         75,420,840
                            //    Geom checker 119,618,014  2,307,939 31,142,922 2,903,245 34,673,978    5,139,741     539,152         86,847,116 18,196,497
                            //    Geom checker 125,554,439  3,431,348 31,900,589 3,706,531 37,272,039    5,160,257     1,670,236       90,573,031 19,505,412
                            //    Geom checker 119,583,440  2,379,307 28,357,175 2,960,173 38,977,837    6,239,242       620,636       86,726,732  9,574,902
    ~Stat() { std::cout << "Geom checker " << ntot<<' '<< nf1<<' '<< nf2 <<' '<< ns1<< ' '<< ns2 << ' ' << ns11 << ' '<< ns21 << ' ' << nth << ' ' << nle <<std::endl;}

  };

  Stat stat;

}

std::pair<bool, TrajectoryStateOnSurface>  
GeomDetCompatibilityChecker::isCompatible(const GeomDet* theDet,
					  const TrajectoryStateOnSurface& tsos,
					  const Propagator& prop, 
					  const MeasurementEstimator& est) {
  stat.ntot++;

  constexpr float tollerance = 2; // in cm
  constexpr float tollerance2 = tollerance*tollerance;
  constexpr float sagCut = 2*tollerance;  // take into account the missing 0.5 below  

  auto err2 = tsos.curvilinearError().matrix()(3,3);
  auto largeErr = err2> 0.1*tollerance2;
  if (largeErr) stat.nle++; 

  auto const & plane = theDet->specificSurface();
  StraightLinePlaneCrossing crossing(tsos.globalPosition().basicVector(),tsos.globalMomentum().basicVector(), prop.propagationDirection());
  auto path = crossing.pathLength(plane);

  auto isIn = path.first;
  float sagita=99999999;
  bool close = false;
  if  unlikely(!path.first) stat.ns1++;
  else {
    auto gpos =  GlobalPoint(crossing.position(path.second));
    auto tpath2 = (gpos-tsos.globalPosition()).perp2();
    // sagitta = d^2*c/2
    sagita = std::abs(tpath2*tsos.globalParameters().transverseCurvature());
    close = sagita<sagCut;
    if (close) { 
       stat.nth++;
       auto pos = plane.toLocal(GlobalPoint(crossing.position(path.second)));
       // auto toll = LocalError(tollerance2,0,tollerance2);
       auto tollL2 = std::max(0.25f*sagita*sagita,0.25f);
       auto toll = LocalError(tollL2,0,tollL2);
       isIn = plane.bounds().inside(pos,toll);
       if (!isIn) { stat.ns2++;
                    if (prop.propagationDirection()==alongMomentum) return std::make_pair( false,TrajectoryStateOnSurface());  
                     // if (!largeErr) return std::make_pair( false,TrajectoryStateOnSurface()); 
                  }
    }
  }

  TrajectoryStateOnSurface && propSt = prop.propagate( tsos, theDet->specificSurface());
  if unlikely ( !propSt.isValid()) { stat.nf1++; return std::make_pair( false, std::move(propSt));}


  auto es = est.estimate( propSt, theDet->specificSurface());
  if (!es) stat.nf2++;
  if (close && (!isIn) && (!es) ) stat.ns11++;
  if (close && es &&(!isIn)) { stat.ns21++; } // std::cout << sagita << std::endl;}
  return std::make_pair( es, std::move(propSt));

}
 
