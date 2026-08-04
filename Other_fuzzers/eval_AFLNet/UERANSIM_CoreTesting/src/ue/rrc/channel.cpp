//
// This file is a part of UERANSIM project.
// Copyright (c) 2023 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include "task.hpp"

#include <lib/rrc/encode.hpp>
#include <ue/app/state_learner.hpp>
#include <ue/rls/task.hpp>

#include <asn/rrc/ASN_RRC_RRCReject.h>
#include <asn/rrc/ASN_RRC_RRCSetup.h>
#include <asn/rrc/ASN_RRC_UL-CCCH-Message.h>
#include <asn/rrc/ASN_RRC_UL-CCCH1-Message.h>
#include <asn/rrc/ASN_RRC_UL-DCCH-Message.h>

#include <sys/stat.h>
#include <fstream>
#include <mutex>

namespace nr::ue
{

static constexpr const char *RRC_PDU_OUTPUT_DIR = "../aflnet/tutorials/corefuzzer/in2";
static std::mutex g_saveMutex;

static const char *GetUlCcchMessageName(const ASN_RRC_UL_CCCH_Message *msg)
{
    if (msg->message.present == ASN_RRC_UL_CCCH_MessageType_PR_c1)
    {
        switch (msg->message.choice.c1->present)
        {
        case ASN_RRC_UL_CCCH_MessageType__c1_PR_rrcSetupRequest:            return "rrcSetupRequest";
        case ASN_RRC_UL_CCCH_MessageType__c1_PR_rrcResumeRequest:           return "rrcResumeRequest";
        case ASN_RRC_UL_CCCH_MessageType__c1_PR_rrcReestablishmentRequest:  return "rrcReestablishmentRequest";
        case ASN_RRC_UL_CCCH_MessageType__c1_PR_rrcSystemInfoRequest:       return "rrcSystemInfoRequest";
        default: break;
        }
    }
    return "unknown_ul_ccch";
}

static const char *GetUlCcch1MessageName(const ASN_RRC_UL_CCCH1_Message *msg)
{
    if (msg->message.present == ASN_RRC_UL_CCCH1_MessageType_PR_c1)
    {
        switch (msg->message.choice.c1->present)
        {
        case ASN_RRC_UL_CCCH1_MessageType__c1_PR_rrcResumeRequest1: return "rrcResumeRequest1";
        default: break;
        }
    }
    return "unknown_ul_ccch1";
}

static const char *GetUlDcchMessageName(const ASN_RRC_UL_DCCH_Message *msg)
{
    if (msg->message.present == ASN_RRC_UL_DCCH_MessageType_PR_c1)
    {
        switch (msg->message.choice.c1->present)
        {
        case ASN_RRC_UL_DCCH_MessageType__c1_PR_measurementReport:             return "measurementReport";
        case ASN_RRC_UL_DCCH_MessageType__c1_PR_rrcReconfigurationComplete:    return "rrcReconfigurationComplete";
        case ASN_RRC_UL_DCCH_MessageType__c1_PR_rrcSetupComplete:              return "rrcSetupComplete";
        case ASN_RRC_UL_DCCH_MessageType__c1_PR_rrcReestablishmentComplete:    return "rrcReestablishmentComplete";
        case ASN_RRC_UL_DCCH_MessageType__c1_PR_rrcResumeComplete:             return "rrcResumeComplete";
        case ASN_RRC_UL_DCCH_MessageType__c1_PR_securityModeComplete:          return "securityModeComplete";
        case ASN_RRC_UL_DCCH_MessageType__c1_PR_securityModeFailure:           return "securityModeFailure";
        case ASN_RRC_UL_DCCH_MessageType__c1_PR_ulInformationTransfer:         return "ulInformationTransfer";
        case ASN_RRC_UL_DCCH_MessageType__c1_PR_locationMeasurementIndication: return "locationMeasurementIndication";
        case ASN_RRC_UL_DCCH_MessageType__c1_PR_ueCapabilityInformation:       return "ueCapabilityInformation";
        case ASN_RRC_UL_DCCH_MessageType__c1_PR_counterCheckResponse:          return "counterCheckResponse";
        case ASN_RRC_UL_DCCH_MessageType__c1_PR_ueAssistanceInformation:       return "ueAssistanceInformation";
        case ASN_RRC_UL_DCCH_MessageType__c1_PR_failureInformation:            return "failureInformation";
        case ASN_RRC_UL_DCCH_MessageType__c1_PR_ulInformationTransferMRDC:     return "ulInformationTransferMRDC";
        case ASN_RRC_UL_DCCH_MessageType__c1_PR_scgFailureInformation:         return "scgFailureInformation";
        case ASN_RRC_UL_DCCH_MessageType__c1_PR_scgFailureInformationEUTRA:    return "scgFailureInformationEUTRA";
        default: break;
        }
    }
    return "unknown_ul_dcch";
}

static void SaveRrcPdu(const OctetString &pdu, int channel, const char *msgName)
{
    // Ensure output directory exists
    mkdir(RRC_PDU_OUTPUT_DIR, 0755);

    std::lock_guard<std::mutex> lock(g_saveMutex);

    std::string filename = std::string(RRC_PDU_OUTPUT_DIR) + "/" + msgName;

    std::ofstream outFile(filename);
    if (!outFile.is_open())
        return;

    // Write format: aflnetRrcMessage_<hex_pdu>:<channel>
    outFile << "aflnetRrcMessage_" << pdu.toHexString() << ":" << channel;
    outFile.close();
}

void UeRrcTask::handleDownlinkRrc(int cellId, rrc::RrcChannel channel, const OctetString &rrcPdu)
{
    if (!hasSignalToCell(cellId))
        return;

    switch (channel)
    {
    case rrc::RrcChannel::BCCH_BCH: {
        auto *pdu = rrc::encode::Decode<ASN_RRC_BCCH_BCH_Message>(asn_DEF_ASN_RRC_BCCH_BCH_Message, rrcPdu);
        if (pdu == nullptr)
            m_logger->err("RRC BCCH-BCH PDU decoding failed.");
        else
            receiveRrcMessage(cellId, pdu);
        asn::Free(asn_DEF_ASN_RRC_BCCH_BCH_Message, pdu);
        break;
    }
    case rrc::RrcChannel::BCCH_DL_SCH: {
        auto *pdu = rrc::encode::Decode<ASN_RRC_BCCH_DL_SCH_Message>(asn_DEF_ASN_RRC_BCCH_DL_SCH_Message, rrcPdu);
        if (pdu == nullptr)
            m_logger->err("RRC BCCH-DL-SCH PDU decoding failed.");
        else
            receiveRrcMessage(cellId, pdu);
        asn::Free(asn_DEF_ASN_RRC_BCCH_DL_SCH_Message, pdu);
        break;
    }
    case rrc::RrcChannel::DL_CCCH: {
        auto *pdu = rrc::encode::Decode<ASN_RRC_DL_CCCH_Message>(asn_DEF_ASN_RRC_DL_CCCH_Message, rrcPdu);
        if (pdu == nullptr)
            m_logger->err("RRC DL-CCCH PDU decoding failed.");
        else
            receiveRrcMessage(cellId, pdu);
        asn::Free(asn_DEF_ASN_RRC_DL_CCCH_Message, pdu);
        break;
    }
    case rrc::RrcChannel::DL_DCCH: {
        if (isActiveCell(cellId))
        {
            auto *pdu = rrc::encode::Decode<ASN_RRC_DL_DCCH_Message>(asn_DEF_ASN_RRC_DL_DCCH_Message, rrcPdu);
            if (pdu == nullptr)
                m_logger->err("RRC DL-DCCH PDU decoding failed.");
            else
                receiveRrcMessage(pdu);
            asn::Free(asn_DEF_ASN_RRC_DL_DCCH_Message, pdu);
        }
        break;
    };
    case rrc::RrcChannel::PCCH: {
        if (isActiveCell(cellId))
        {
            auto *pdu = rrc::encode::Decode<ASN_RRC_PCCH_Message>(asn_DEF_ASN_RRC_PCCH_Message, rrcPdu);
            if (pdu == nullptr)
                m_logger->err("RRC PCCH PDU decoding failed.");
            else
                receiveRrcMessage(pdu);
            asn::Free(asn_DEF_ASN_RRC_PCCH_Message, pdu);
        }
        break;
    }
    case rrc::RrcChannel::UL_CCCH:
    case rrc::RrcChannel::UL_CCCH1:
    case rrc::RrcChannel::UL_DCCH:
        break;
    }
}

void UeRrcTask::sendRrcMessage(int cellId, ASN_RRC_UL_CCCH_Message *msg)
{
    OctetString pdu = rrc::encode::EncodeS(asn_DEF_ASN_RRC_UL_CCCH_Message, msg);
    if (pdu.length() == 0)
    {
        m_logger->err("RRC UL-CCCH encoding failed.");
        return;
    }

    SaveRrcPdu(pdu, static_cast<int>(rrc::RrcChannel::UL_CCCH), GetUlCcchMessageName(msg));

    auto m = std::make_unique<NmUeRrcToRls>(NmUeRrcToRls::RRC_PDU_DELIVERY);
    m->cellId = cellId;
    m->channel = rrc::RrcChannel::UL_CCCH;
    m->pdu = std::move(pdu);
    m_base->rlsTask->push(std::move(m));
}

void UeRrcTask::sendRrcMessage(int cellId, ASN_RRC_UL_CCCH1_Message *msg)
{
    OctetString pdu = rrc::encode::EncodeS(asn_DEF_ASN_RRC_UL_CCCH1_Message, msg);
    if (pdu.length() == 0)
    {
        m_logger->err("RRC UL-CCCH1 encoding failed.");
        return;
    }

    SaveRrcPdu(pdu, static_cast<int>(rrc::RrcChannel::UL_CCCH1), GetUlCcch1MessageName(msg));

    auto m = std::make_unique<NmUeRrcToRls>(NmUeRrcToRls::RRC_PDU_DELIVERY);
    m->cellId = cellId;
    m->channel = rrc::RrcChannel::UL_CCCH1;
    m->pdu = std::move(pdu);
    m_base->rlsTask->push(std::move(m));
}

void UeRrcTask::sendRrcMessage(ASN_RRC_UL_DCCH_Message *msg)
{
    OctetString pdu = rrc::encode::EncodeS(asn_DEF_ASN_RRC_UL_DCCH_Message, msg);
    if (pdu.length() == 0)
    {
        m_logger->err("RRC UL-DCCH encoding failed.");
        return;
    }

    SaveRrcPdu(pdu, static_cast<int>(rrc::RrcChannel::UL_DCCH), GetUlDcchMessageName(msg));

    auto m = std::make_unique<NmUeRrcToRls>(NmUeRrcToRls::RRC_PDU_DELIVERY);
    m->cellId = m_base->shCtx.currentCell.get<int>([](auto &value) { return value.cellId; });
    m->channel = rrc::RrcChannel::UL_DCCH;
    m->pdu = std::move(pdu);
    m_base->rlsTask->push(std::move(m));
}

void UeRrcTask::receiveRrcMessage(int cellId, ASN_RRC_BCCH_BCH_Message *msg)
{
    if (msg->message.present == ASN_RRC_BCCH_BCH_MessageType_PR_mib)
        receiveMib(cellId, *msg->message.choice.mib);
}

void UeRrcTask::receiveRrcMessage(int cellId, ASN_RRC_BCCH_DL_SCH_Message *msg)
{
    if (msg->message.present != ASN_RRC_BCCH_DL_SCH_MessageType_PR_c1)
        return;

    auto &c1 = msg->message.choice.c1;
    switch (c1->present)
    {
    case ASN_RRC_BCCH_DL_SCH_MessageType__c1_PR_systemInformationBlockType1:
        receiveSib1(cellId, *c1->choice.systemInformationBlockType1);
        break;
    default:
        break;
    }
}

void UeRrcTask::receiveRrcMessage(int cellId, ASN_RRC_DL_CCCH_Message *msg)
{
    if (msg->message.present != ASN_RRC_DL_CCCH_MessageType_PR_c1)
        return;

    auto &c1 = msg->message.choice.c1;
    switch (c1->present)
    {
    case ASN_RRC_DL_CCCH_MessageType__c1_PR_rrcReject:
        if (state_learner->rrcFuzzing)
            state_learner->notify_response("rrcReject");
        receiveRrcReject(cellId, *c1->choice.rrcReject);
        break;
    case ASN_RRC_DL_CCCH_MessageType__c1_PR_rrcSetup:
        if (state_learner->rrcFuzzing)
            state_learner->notify_response("rrcSetup");
        receiveRrcSetup(cellId, *c1->choice.rrcSetup);
        break;
    default:
        break;
    }
}

void UeRrcTask::receiveRrcMessage(ASN_RRC_DL_DCCH_Message *msg)
{
    if (msg->message.present != ASN_RRC_DL_DCCH_MessageType_PR_c1)
        return;

    auto &c1 = msg->message.choice.c1;
    switch (c1->present)
    {
    case ASN_RRC_DL_DCCH_MessageType__c1_PR_dlInformationTransfer:
        if (state_learner->rrcFuzzing)
            state_learner->notify_response("dlInformationTransfer");
        receiveDownlinkInformationTransfer(*c1->choice.dlInformationTransfer);
        break;
    case ASN_RRC_DL_DCCH_MessageType__c1_PR_rrcRelease:
        if (state_learner->rrcFuzzing)
            state_learner->notify_response("rrcRelease");
        receiveRrcRelease(*c1->choice.rrcRelease);
        break;
    default:
        break;
    }
}

void UeRrcTask::receiveRrcMessage(ASN_RRC_PCCH_Message *msg)
{
    if (msg->message.present != ASN_RRC_PCCH_MessageType_PR_c1)
        return;

    auto &c1 = msg->message.choice.c1;
    switch (c1->present)
    {
    case ASN_RRC_PCCH_MessageType__c1_PR_paging:
        receivePaging(*c1->choice.paging);
        break;
    default:
        break;
    }
}

} // namespace nr::ue
