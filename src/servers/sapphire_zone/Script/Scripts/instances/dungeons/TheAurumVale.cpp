#include <Script/NativeScriptApi.h>
#include <ScriptObject.h>
#include <Zone/InstanceContent.h>

class TheAurumVale : public InstanceContentScript
{
public:
   TheAurumVale() : InstanceContentScript( 5 )
   { }

   void onInit( InstanceContentPtr instance ) override
   {
      
      instance->registerEObj( "Entrance", 2000182, 4246303, 5, { 158.558304f, -0.023911f, 81.249123f }, 1.000000f );
      instance->registerEObj( "Exit", 2000139, 0, 4, { -410.069702f, -33.063400f, -116.395897f }, 0.991760f );
      instance->registerEObj( "Shortcut", 2000700, 0, 4, { 151.384796f, -1.596331f, 86.655884f }, 0.991760f );
      instance->registerEObj( "Unknown1", 2000272, 3643401, 4, { -44.571659f, -16.218571f, -112.108101f }, 0.991760f );
      instance->registerEObj( "Unknown2", 2000779, 4259184, 4, { 58.213131f, -9.300029f, -0.564636f }, 0.991760f );
      instance->registerEObj( "Unknown3", 2000780, 4259186, 4, { 58.539280f, -8.801913f, 4.282087f }, 1.000000f );
      instance->registerEObj( "Unknown4", 2000785, 4259185, 4, { 34.952770f, -9.096572f, -23.527361f }, 1.000000f );
      instance->registerEObj( "Unknown5", 2000273, 4248891, 4, { -333.089203f, -32.733212f, -161.730392f }, 0.991760f );
      instance->registerEObj( "Unknown6", 2000781, 4259189, 4, { -127.598099f, -30.310749f, -139.019302f }, 1.000000f );
      instance->registerEObj( "Unknown7", 2000782, 4259188, 4, { -126.660500f, -30.285540f, -134.096695f }, 1.000000f );
      instance->registerEObj( "Unknown8", 2000786, 4259190, 4, { -169.906296f, -27.704550f, -169.152496f }, 1.000000f );
      instance->registerEObj( "Unknown9", 2000783, 4246452, 4, { -374.789612f, -32.153561f, -140.808197f }, 1.000000f );
      instance->registerEObj( "Unknown10", 2000784, 4246451, 4, { -372.679901f, -32.341091f, -136.354401f }, 1.000000f );
   }

   void onUpdate( InstanceContentPtr instance, uint32_t currTime ) override
   {

   }

   void onEnterTerritory( Entity::Player &player, uint32_t eventId, uint16_t param1, uint16_t param2 ) override
   {

   }

};