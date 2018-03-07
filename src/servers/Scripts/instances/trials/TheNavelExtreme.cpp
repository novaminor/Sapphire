#include <ScriptObject.h>
#include <Zone/InstanceContent.h>

class TheNavelExtreme : public InstanceContentScript
{
public:
   TheNavelExtreme() : InstanceContentScript( 20009 )
   { }

   void onInit( InstanceContentPtr instance ) override
   {
      instance->registerEObj( "Entrance", 2000182, 4236868, 5, { 0.000000f, 0.000000f, 24.000000f }, 1.000000f, 0.000000f ); 
      // States -> vf_lock_on vf_lock_of 
      instance->registerEObj( "Exit", 2000139, 0, 4, { 0.000000f, 0.000000f, -10.000000f }, 1.000000f, 0.000000f ); 

   }

   void onUpdate( InstanceContentPtr instance, uint32_t currTime ) override
   {

   }

   void onEnterTerritory( Entity::Player &player, uint32_t eventId, uint16_t param1, uint16_t param2 ) override
   {

   }

};