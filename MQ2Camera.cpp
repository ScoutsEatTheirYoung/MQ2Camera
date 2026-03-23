/*
 * MQ2Camera - Exposes EverQuest camera data to the MQ2 TLO system.
 *
 * Usage in macros:
 *   /echo ${Camera.X}
 *   /echo ${Camera.Y}
 *   /echo ${Camera.Z}
 *   /echo ${Camera.Heading}
 *   /echo ${Camera.Pitch}
 *   /echo ${Camera.Roll}
 *   /echo ${Camera}           -- prints "X=... Y=... Z=... H=..."
 */

#include <mq/Plugin.h>
#include "eqlib/graphics/CameraInterface.h"

using namespace mq::datatypes;

PLUGIN_VERSION(1.0);
PreSetup("MQ2Camera");

// ── Camera type ────────────────────────────────────────────────────────────

class MQ2CameraType;
MQ2CameraType* pCameraType = nullptr;

class MQ2CameraType : public MQ2Type
{
public:
    enum class CameraMembers
    {
        X,
        Y,
        Z,
        Heading,
        Pitch,
        Roll,
    };

    MQ2CameraType() : MQ2Type("Camera")
    {
        ScopedTypeMember(CameraMembers, X);
        ScopedTypeMember(CameraMembers, Y);
        ScopedTypeMember(CameraMembers, Z);
        ScopedTypeMember(CameraMembers, Heading);
        ScopedTypeMember(CameraMembers, Pitch);
        ScopedTypeMember(CameraMembers, Roll);
    }

    bool GetMember(MQVarPtr VarPtr, const char* Member, char* Index, MQTypeVar& Dest) override
    {
        if (!pDisplay || !pDisplay->pCamera)
            return false;

        MQTypeMember* pMember = MQ2CameraType::FindMember(Member);
        if (!pMember)
            return false;

        CCameraInterface* cam = pDisplay->pCamera;

        switch (static_cast<CameraMembers>(pMember->ID))
        {
        case CameraMembers::X:
            Dest.Float = cam->GetX();
            Dest.Type = pFloatType;
            return true;

        case CameraMembers::Y:
            Dest.Float = cam->GetY();
            Dest.Type = pFloatType;
            return true;

        case CameraMembers::Z:
            Dest.Float = cam->GetZ();
            Dest.Type = pFloatType;
            return true;

        case CameraMembers::Heading:
            Dest.Float = cam->GetHeading();
            Dest.Type = pFloatType;
            return true;

        case CameraMembers::Pitch:
            Dest.Float = cam->GetPitch();
            Dest.Type = pFloatType;
            return true;

        case CameraMembers::Roll:
            Dest.Float = cam->GetRoll();
            Dest.Type = pFloatType;
            return true;
        }

        return false;
    }

    bool ToString(MQVarPtr VarPtr, char* Destination) override
    {
        if (!pDisplay || !pDisplay->pCamera)
        {
            strcpy_s(Destination, MAX_STRING, "NULL");
            return true;
        }

        CCameraInterface* cam = pDisplay->pCamera;
        sprintf_s(Destination, MAX_STRING, "X=%.4f Y=%.4f Z=%.4f H=%.4f",
            cam->GetX(), cam->GetY(), cam->GetZ(), cam->GetHeading());
        return true;
    }

    static bool dataCamera(const char* szIndex, MQTypeVar& Ret)
    {
        Ret.DWord = 1;
        Ret.Type = pCameraType;
        return true;
    }
};

// ── Plugin lifecycle ───────────────────────────────────────────────────────

PLUGIN_API void InitializePlugin()
{
    DebugSpewAlways("MQ2Camera::InitializePlugin");
    pCameraType = new MQ2CameraType();
    AddMQ2Type(*pCameraType);
    AddMQ2Data("Camera", MQ2CameraType::dataCamera);
    WriteChatf("\a[MQ2Camera]\ax loaded - \ay${Camera.X}\ax / \ay${Camera.Y}\ax / \ay${Camera.Z}");
}

PLUGIN_API void ShutdownPlugin()
{
    DebugSpewAlways("MQ2Camera::ShutdownPlugin");
    RemoveMQ2Data("Camera");
    RemoveMQ2Type(*pCameraType);
    delete pCameraType;
    pCameraType = nullptr;
}
