/*
 * COPYRIGHT 2015 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 1-Oct-2015
 */

#include "s3_action_base.h"
#include "s3_clovis_layout.h"
#include "s3_error_codes.h"
#include "s3_option.h"
#include "s3_stats.h"

S3Action::S3Action(std::shared_ptr<S3RequestObject> req, bool check_shutdown,
                   std::shared_ptr<S3AuthClientFactory> auth_factory,
                   bool skip_auth)
    : Action(req, check_shutdown, auth_factory, skip_auth), request(req) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");
  setup_steps();
}

S3Action::~S3Action() { s3_log(S3_LOG_DEBUG, request_id, "Destructor\n"); }

void S3Action::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setup the action\n");
  s3_log(S3_LOG_DEBUG, request_id,
         "S3Option::is_auth_disabled: (%d), skip_auth: (%d)\n",
         S3Option::get_instance()->is_auth_disabled(), skip_auth);

  if (!S3Option::get_instance()->is_auth_disabled() && !skip_auth) {
    // add_task(std::bind( &S3Action::fetch_acl_policies, this ));
    // Commented till we implement Authorization feature completely.
    // Current authorisation implementation in AuthServer is partial
    add_task(std::bind(&S3Action::check_authorization, this));
  }
}

// TODO -- When this function is enabled, for object we need to
// first fetch bucket details and then provide object list index oid to the
// constructor
// of S3Objectmetadata else load() will result in crash
//
// void S3Action::fetch_acl_policies() {
//  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
//  if (request->get_api_type() == S3ApiType::object) {
//    object_metadata = std::make_shared<S3ObjectMetadata>(request);
//    object_metadata->load(std::bind( &S3Action::next, this), std::bind(
//    &S3Action::fetch_acl_object_policies_failed, this));
//  } else if (request->get_api_type() == S3ApiType::bucket) {
//    bucket_metadata = std::make_shared<S3BucketMetadata>(request);
//    bucket_metadata->load(std::bind( &S3Action::next, this), std::bind(
//    &S3Action::fetch_acl_bucket_policies_failed, this));
//  } else {
//    next();
//  }
//}

// void S3Action::fetch_acl_object_policies_failed() {
//  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
//  if (object_metadata->get_state() != S3ObjectMetadataState::missing) {
//    s3_log(S3_LOG_ERROR, request_id, "Metadata lookup error: failed to load
// acl/policies
//    from object\n");
//    S3Error error("InternalError", request->get_request_id(),
//    request->get_object_uri());
//    std::string& response_xml = error.to_xml();
//    request->set_out_header_value("Content-Type", "application/xml");
//    request->set_out_header_value("Content-Length",
//    std::to_string(response_xml.length()));
//    request->send_response(error.get_http_status_code(), response_xml);

//    done();
//    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
//    i_am_done();
//  } else {
//    next();
//  }
//}

// void S3Action::fetch_acl_bucket_policies_failed() {
//  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
//  if (bucket_metadata->get_state() != S3BucketMetadataState::missing) {
//    s3_log(S3_LOG_ERROR, request_id, "Metadata lookup error: failed to load
// acl/policies
//    from bucket\n");
//    S3Error error("InternalError", request->get_request_id(),
//    request->get_object_uri());
//    std::string& response_xml = error.to_xml();
//    request->set_out_header_value("Content-Type", "application/xml");
//    request->set_out_header_value("Content-Length",
//    std::to_string(response_xml.length()));
//    request->send_response(error.get_http_status_code(), response_xml);

//    done();
//    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
//    i_am_done();
//  } else {
//    next();
//  }
//}

void S3Action::check_authorization() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  // TODO Authorization is No op currently
  // will enable once acl and polcy is implemented
  // if (request->get_api_type() == S3ApiType::bucket) {
  //  auth_client->set_acl_and_policy(bucket_metadata->get_encoded_bucket_acl(),
  //                                  bucket_metadata->get_policy_as_json());
  // } else if (request->get_api_type() == S3ApiType::object) {
  //  auth_client->set_acl_and_policy(object_metadata->get_encoded_object_acl(),
  //                                   "");
  // }
  auth_client->check_authorization(
      std::bind(&S3Action::check_authorization_successful, this),
      std::bind(&S3Action::check_authorization_failed, this));
}

void S3Action::check_authorization_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3Action::check_authorization_failed() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  if (request->client_connected()) {
    std::string error_code = auth_client->get_error_code();
    if (error_code == "InvalidAccessKeyId") {
      s3_stats_inc("authorization_failed_invalid_accesskey_count");
    } else if (error_code == "SignatureDoesNotMatch") {
      s3_stats_inc("authorization_failed_signature_mismatch_count");
    }
    s3_log(S3_LOG_ERROR, request_id, "Authorization failure: %s\n",
           error_code.c_str());
    request->respond_error(error_code);
  }
  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  i_am_done();
}