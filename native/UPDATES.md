# Ratio updates (0.2.1 / build 3)

Install the pinned Sparkle dependency with `./setup-sparkle.sh`, then run `./build.sh`. This creates an ad-hoc development app and runs the accounting self-tests. Production apps require Developer ID signing and notarization, including Sparkle's nested helpers.

## Buyer flow

Enter the purchase email and six-digit emailed code inside the popover. The server checks the paid Ratio order before sending a code. Codes expire after ten minutes and have five attempts; successful verification consumes the code atomically. The Mac stores its individual opaque credential in Keychain. There is no shared download password in the app.

Right-click the menu-bar ratio for **Check for Updates…** or **Update Account…**. Sparkle checks in the background after sign-in and installs updates in place. Users of releases before 0.2.0 need one manual replacement to get the updater. Tracking data and rules remain in UserDefaults under the unchanged bundle identifier.

## Backend

The private VV store repository owns `/api/ratio/auth/request`, `/api/ratio/auth/verify`, and `/api/ratio/updates/[asset]`. Its server-only `RATIO_AUTH_SECRET` protects code hashes; the database stores hashed device tokens. All update requests recheck purchase status, including refunds and disputes. `ratio_devices.revoked_at` revokes one device. Sending codes requires the existing VV Resend configuration.

## Release checklist

1. Increment `CFBundleVersion` and `CFBundleShortVersionString` in `Ratio.app/Contents/Info.plist`; never change the bundle identifier.
2. Build and stage the app. Developer ID sign Sparkle's Downloader.xpc, Installer.xpc, Autoupdate, Updater.app, and framework from inside out, preserving helper entitlements and enabling hardened runtime. Then sign Ratio with `entitlements.plist`.
3. Create a versioned DMG with an Applications symlink. Sign, notarize with the `ratio-notary` Keychain profile, staple, and verify it.
4. Generate the appcast with Sparkle's `generate_appcast --account visualizevalue-ratio --maximum-deltas 0 --download-url-prefix https://visualizevalue.com/api/ratio/updates/ ARCHIVES`. The Ed25519 private key stays in the release Mac's Keychain; only its public key is in Info.plist. Signed feeds and archive verification are required.
5. Preserve previous versioned archives. Copy the new archive and signed appcast into VV store's `private/ratio/updates/`, and copy the same DMG to `private/ratio/Ratio.dmg` for new purchases. Publish them together. Never change an already published versioned archive or edit a signed appcast by hand.
6. Test a signed old-to-new installation in an isolated writable app copy and verify the installed bundle's code signature. Keep test feeds separate from production.

Validation for this release: accounting self-tests, 35 backend auth/commerce tests, clean production build, accepted Apple notarization, and real Sparkle installation from build 1 to build 2 with the installed signature verified. Live email delivery still requires testing with a purchaser's mailbox.
