#include "rigid_body.h"

namespace pmk
{
    void RigidBodyContext::Initialize(Scene* scene)
    {
        scene_ = scene;
    }

    void RigidBodyContext::CleanUp()
    {
    }
}
