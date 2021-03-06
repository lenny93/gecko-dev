/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Constructor(DOMString errorName, short retryCount),
 Pref="dom.icc.enabled",
 CheckPermissions="mobileconnection",
 AvailableIn="CertifiedApps"]
interface IccCardLockError : DOMError {
  readonly attribute short retryCount;
};
