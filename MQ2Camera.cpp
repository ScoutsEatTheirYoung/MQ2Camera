/*
 * MQ2Camera - Exposes EverQuest camera data to the MQ2 TLO system.
 *
 * Basic members:
 *   /echo ${Camera.X}          -- camera position X
 *   /echo ${Camera.Y}          -- camera position Y
 *   /echo ${Camera.Z}          -- camera position Z
 *   /echo ${Camera.Heading}    -- degrees (0=N, 90=E, 180=S, 270=W)
 *   /echo ${Camera.Pitch}      -- degrees (positive=up, ~85 max)
 *   /echo ${Camera.Roll}       -- degrees
 *   /echo ${Camera.FOV}        -- vertical field of view in degrees
 *   /echo ${Camera.ScreenW}    -- render buffer width in pixels
 *   /echo ${Camera.ScreenH}    -- render buffer height in pixels
 *   /echo ${Camera}            -- "X=... Y=... Z=... H=... P=... FOV=..."
 *   /camera                    -- prints all values to chat
 *
 * World-to-screen projection (Index = "worldX,worldY,worldZ"):
 *   /echo ${Camera.Project[x,y,z]}         -- "sx,sy" or FALSE if behind camera
 *   /echo ${Camera.ProjectClamped[x,y,z]}  -- "sx,sy", clamped to nearest screen edge
 *   /echo ${Camera.ProjectX[x,y,z]}        -- screen X float, -1 if behind camera
 *   /echo ${Camera.ProjectY[x,y,z]}        -- screen Y float, -1 if behind camera
 *   /echo ${Camera.ProjectVisible[x,y,z]}  -- TRUE only if on screen and in front of camera
 */

#include <mq/Plugin.h>
#include "eqlib/graphics/CameraInterface.h"

using namespace mq::datatypes;
using namespace eqlib;

// EQ uses a 512-unit circle internally. 512 units = 360 degrees.
static constexpr float EQ_TO_DEG = 360.0f / 512.0f;

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
        FOV,
        ScreenW,
        ScreenH,
        Project,
        ProjectClamped,
        ProjectX,
        ProjectY,
        ProjectVisible,
    };

    MQ2CameraType() : MQ2Type("Camera")
    {
        ScopedTypeMember(CameraMembers, X);
        ScopedTypeMember(CameraMembers, Y);
        ScopedTypeMember(CameraMembers, Z);
        ScopedTypeMember(CameraMembers, Heading);
        ScopedTypeMember(CameraMembers, Pitch);
        ScopedTypeMember(CameraMembers, Roll);
        ScopedTypeMember(CameraMembers, FOV);
        ScopedTypeMember(CameraMembers, ScreenW);
        ScopedTypeMember(CameraMembers, ScreenH);
        ScopedTypeMember(CameraMembers, Project);
        ScopedTypeMember(CameraMembers, ProjectClamped);
        ScopedTypeMember(CameraMembers, ProjectX);
        ScopedTypeMember(CameraMembers, ProjectY);
        ScopedTypeMember(CameraMembers, ProjectVisible);
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
            Dest.Float = cam->GetHeading() * EQ_TO_DEG;
            Dest.Type = pFloatType;
            return true;

        case CameraMembers::Pitch:
            Dest.Float = cam->GetPitch() * EQ_TO_DEG;
            Dest.Type = pFloatType;
            return true;

        case CameraMembers::Roll:
            Dest.Float = cam->GetRoll() * EQ_TO_DEG;
            Dest.Type = pFloatType;
            return true;

        case CameraMembers::FOV:
        {
            CCamera* ccam = cam->AsCamera();
            if (!ccam) return false;
            // halfViewAngle is already in degrees; full FOV = half * 2
            Dest.Float = ccam->halfViewAngle * 2.0f;
            Dest.Type = pFloatType;
            return true;
        }

        case CameraMembers::ScreenW:
            Dest.Float = cam->GetRenderBufferWidth();
            Dest.Type = pFloatType;
            return true;

        case CameraMembers::ScreenH:
            Dest.Float = cam->GetRenderBufferHeight();
            Dest.Type = pFloatType;
            return true;

        case CameraMembers::Project:
        {
            float wx, wy, wz;
            if (!Index || sscanf_s(Index, "%f,%f,%f", &wx, &wy, &wz) != 3)
                return false;
            float sx, sy;
            if (!cam->ProjectWorldCoordinatesToScreen(CVector3(wx, wy, wz), sx, sy))
                return false;
            static char szProject[64];
            sprintf_s(szProject, "%.2f,%.2f", sx, sy);
            Dest.Ptr = szProject;
            Dest.Type = pStringType;
            return true;
        }

        case CameraMembers::ProjectClamped:
        {
            float wx, wy, wz;
            if (!Index || sscanf_s(Index, "%f,%f,%f", &wx, &wy, &wz) != 3)
                return false;
            float sx, sy;
            // Project first (result ignored — we always clamp)
            cam->ProjectWorldCoordinatesToScreen(CVector3(wx, wy, wz), sx, sy);
            float cx, cy;
            cam->GetClampedScreenCoordinates(cx, cy, sx, sy);
            static char szProjectClamped[64];
            sprintf_s(szProjectClamped, "%.2f,%.2f", cx, cy);
            Dest.Ptr = szProjectClamped;
            Dest.Type = pStringType;
            return true;
        }

        case CameraMembers::ProjectX:
        {
            float wx, wy, wz;
            if (!Index || sscanf_s(Index, "%f,%f,%f", &wx, &wy, &wz) != 3)
                return false;
            float sx, sy;
            if (!cam->ProjectWorldCoordinatesToScreen(CVector3(wx, wy, wz), sx, sy))
            {
                Dest.Float = -1.0f;
                Dest.Type = pFloatType;
                return true;
            }
            Dest.Float = sx;
            Dest.Type = pFloatType;
            return true;
        }

        case CameraMembers::ProjectY:
        {
            float wx, wy, wz;
            if (!Index || sscanf_s(Index, "%f,%f,%f", &wx, &wy, &wz) != 3)
                return false;
            float sx, sy;
            if (!cam->ProjectWorldCoordinatesToScreen(CVector3(wx, wy, wz), sx, sy))
            {
                Dest.Float = -1.0f;
                Dest.Type = pFloatType;
                return true;
            }
            Dest.Float = sy;
            Dest.Type = pFloatType;
            return true;
        }

        case CameraMembers::ProjectVisible:
        {
            float wx, wy, wz;
            if (!Index || sscanf_s(Index, "%f,%f,%f", &wx, &wy, &wz) != 3)
                return false;
            float sx, sy;
            bool inFront = cam->ProjectWorldCoordinatesToScreen(CVector3(wx, wy, wz), sx, sy);
            float w = cam->GetRenderBufferWidth();
            float h = cam->GetRenderBufferHeight();
            Dest.DWord = inFront && sx >= 0.0f && sx <= w && sy >= 0.0f && sy <= h ? 1 : 0;
            Dest.Type = pBoolType;
            return true;
        }
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
        float fov = 0.f;
        if (CCamera* ccam = cam->AsCamera())
            fov = ccam->halfViewAngle * 2.0f;

        sprintf_s(Destination, MAX_STRING, "X=%.4f Y=%.4f Z=%.4f H=%.2f P=%.2f FOV=%.1f",
            cam->GetX(), cam->GetY(), cam->GetZ(),
            cam->GetHeading() * EQ_TO_DEG, cam->GetPitch() * EQ_TO_DEG, fov);
        return true;
    }

    static bool dataCamera(const char* szIndex, MQTypeVar& Ret)
    {
        Ret.DWord = 1;
        Ret.Type = pCameraType;
        return true;
    }
};

// ── /camera command ────────────────────────────────────────────────────────

static void CmdCamera(PlayerClient* /*pChar*/, const char* /*szLine*/)
{
    if (!pDisplay || !pDisplay->pCamera)
    {
        WriteChatf("\a[MQ2Camera]\ax No camera available.");
        return;
    }

    CCameraInterface* cam = pDisplay->pCamera;

    float fov = 0.f;
    if (CCamera* ccam = cam->AsCamera())
        fov = ccam->halfViewAngle * 2.0f;

    WriteChatf("\a[MQ2Camera]\ax --- Camera Info ---");
    WriteChatf("  Position : \ayX=%.4f  Y=%.4f  Z=%.4f", cam->GetX(), cam->GetY(), cam->GetZ());
    WriteChatf("  Angles   : \ayHeading=%.2f\xb0  Pitch=%.2f\xb0  Roll=%.2f\xb0",
        cam->GetHeading() * EQ_TO_DEG, cam->GetPitch() * EQ_TO_DEG, cam->GetRoll() * EQ_TO_DEG);
    WriteChatf("  FOV      : \ay%.2f\xb0", fov);
    WriteChatf("  Screen   : \ay%.0f x %.0f", cam->GetRenderBufferWidth(), cam->GetRenderBufferHeight());
}

// ── Plugin lifecycle ───────────────────────────────────────────────────────

PLUGIN_API void InitializePlugin()
{
    DebugSpewAlways("MQ2Camera::InitializePlugin");
    pCameraType = new MQ2CameraType();
    AddMQ2Type(*pCameraType);
    AddMQ2Data("Camera", MQ2CameraType::dataCamera);
    AddCommand("/camera", CmdCamera);
    WriteChatf("\a[MQ2Camera]\ax loaded - type \ay/camera\ax for info");
}

PLUGIN_API void ShutdownPlugin()
{
    DebugSpewAlways("MQ2Camera::ShutdownPlugin");
    RemoveCommand("/camera");
    RemoveMQ2Data("Camera");
    RemoveMQ2Type(*pCameraType);
    delete pCameraType;
    pCameraType = nullptr;
}
