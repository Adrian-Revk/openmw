#ifndef CSM_TOOLS_BIRTHSIGNCHECK_H
#define CSM_TOOLS_BIRTHSIGNCHECK_H

#include <components/esm/loadbsgn.hpp>

#include "../world/idcollection.hpp"

#include "stage.hpp"

namespace CSMTools
{
    /// \brief VerifyStage: make sure that birthsign records are internally consistent
    class BirthsignCheckStage : public Stage
    {
            const CSMWorld::IdCollection<ESM::BirthSign>& mBirthsigns;

        public:

            BirthsignCheckStage (const CSMWorld::IdCollection<ESM::BirthSign>& birthsigns);

            virtual int setup();
            ///< \return number of steps

            virtual void perform (int stage, std::vector<std::string>& messages);
            ///< Messages resulting from this tage will be appended to \a messages.
    };
}

#endif
