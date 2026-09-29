#include "ar61850/dms/engine.hpp"

#include <iostream>

int main() {
    ar61850::dms::Session session;
    session.set_state(ar61850::dms::AssociationState::Connecting);
    session.set_associate_id("lab-cp1");
    session.set_state(ar61850::dms::AssociationState::Associated);

    std::cout
        << "AR61850 DMS core ready\n"
        << "associateId=" << session.associate_id() << "\n"
        << "nextInvokeId=" << session.next_invoke_id() << "\n";

    return 0;
}
