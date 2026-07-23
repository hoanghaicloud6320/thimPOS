# ThimPOS — Current System Specification

This document describes the complete system represented by the current source.
Endpoint-level request and response details are frozen in the `contract_*.md`
documents.

## 1. Product scope

ThimPOS is a Windows-oriented point-of-sale server and browser UI for a small
shop. It provides:

- public product browsing and manager product maintenance;
- authenticated order entry, replacement, deletion and bill printing;
- separate credential/session and account/profile management;
- manager-only inventory, purchase lots, recipes and movement ledger;
- manager-only AI-assisted business reports using license-provided Gemini
  configuration;
- authenticated shared file library with public static file URLs;
- change audit logging for selected business mutations;
- cloud license validation before the application starts serving.

The application uses SQLite by default and serves its web UI and static assets
through Drogon.

## 2. Architecture and boundaries

The normal request flow is:

```text
HTTP request
  -> AuthNFilter (when required: resolve sid to username)
  -> AuthZFilter (when required: resolve account role)
  -> Controller (HTTP validation and response contract)
  -> Plugin/Service (business rules and orchestration)
  -> Repository or filesystem/provider adapter
```

Design rules:

- A Drogon plugin is the public service boundary for a domain.
- Controllers select filters and implement the HTTP contract.
- Services own validation, orchestration and domain behavior.
- Repositories use Drogon's database client and raw SQL; there is no ORM layer.
- SQLite and the filesystem are local sources of truth.
- Coroutine APIs are preferred where the underlying interface supports them.
- Credentials and accounts are deliberately separate records and are not
  implicitly synchronized.

## 3. Modules

### Products

`ProductManagerController` exposes public read/list/search endpoints and
manager-only create/update/soft-delete endpoints. `ProductManagerService` owns
validation; `ProductRepo` owns the `products` table. Prices are integer minor
units (VND in the bundled UI), and deletion sets `is_active = false`.

Contract: [contract_products.md](contract_products.md).

### Orders and printing

`OrderController` permits `manager` and `staff` accounts to create, read,
list, fully replace and delete orders. `OrderService` coordinates
`OrderRepo` and inventory synchronization. Order items store unit-price
snapshots so later product price changes do not rewrite sales totals.

`BillPrintController` renders an existing order using
`bills/activeTemplate.tpl`, saves a text bill and delegates printing to
`TxtPrinterService`.

Contract: [contract_orders.md](contract_orders.md).

### Inventory

`InventoryController` is manager-only. `InventoryService` and `InventoryRepo`
manage inventory items, purchase lots, product recipes and an append-oriented
movement ledger. On-hand values are sums of movements. Order create, replace
and delete operations create or reconcile recipe-based movements.

Contract: [contract_inventory.md](contract_inventory.md).

### Authentication and accounts

`AuthNService` manages credentials and multi-device sessions in
`credentials` and `sessions`. The `sid` cookie is the authentication token.
`AuthNFilter` validates it and attaches `username`.

`AccountManagementService` manages profile and role data in `accounts`.
`AuthZFilter` resolves the authenticated username to an account and attaches
its role. A valid credential without a corresponding account can authenticate
but cannot pass authorization.

Contracts: [contract_authn.md](contract_authn.md) and
[contract_account.md](contract_account.md).

### AI

`AiController` is manager-only and stateless. `AiService` sends the supplied
message history to Gemini. API key and model are read from verified license
metadata, never accepted from the HTTP request. The browser assembles sales and
inventory context using existing APIs.

Contract: [contract_ai.md](contract_ai.md).

### Shared file library

`LibraryService` stores files under `static/user_library_files`, makes upload
names safe and collision-free, and reads metadata directly from the filesystem.
Authenticated users can list, upload and delete; the resulting static URLs are
public by design.

Contract: [contract_library.md](contract_library.md).

### Audit log

Change-audit helpers record selected successful update/delete operations with
the actor, target, before/after values and whitelisted changed fields. Read-only
requests and failed authentication attempts are not change-audited.

Details: [audit_logging.md](audit_logging.md).

### Licensing

`KeyManagerClient` validates activation with the cloud KeyManager before normal
startup. Release builds use the Windows native trust store for HTTPS and must
pass a fresh activation test from the packaged directory.

Details: [license_check.md](license_check.md).

## 4. Access model

| Area | Public | Authenticated staff | Manager |
|---|---:|---:|---:|
| Product read/list/search | Yes | Yes | Yes |
| Product create/update/delete | No | No | Yes |
| Orders and bill printing | No | Yes | Yes |
| Personal profile read | No | Yes | Yes |
| Personal profile update | No | No | Yes |
| Account/credential administration | No | No | Yes |
| Inventory and movement data | No | No | Yes |
| AI generation | No | No | Yes |
| File library management | No | Yes | Yes |
| Library static URLs | Yes, with URL | Yes | Yes |

## 5. Persistence

SQLite is configured in `config.json`. Tables are initialized by their owning
plugins and are also summarized in `schema.sql`. Principal groups are:

- `products`;
- `orders`, `order_items`;
- `credentials`, `sessions`;
- `accounts`;
- inventory items, lots, recipes and movements.

Runtime data such as `csdl.sqlite3`, logs, generated bills, uploaded user files
and license cache are not source artifacts and must not be bundled into a clean
release unless explicitly listed by the release checklist.

## 6. Configuration and external dependencies

- Drogon/Trantor: HTTP server, filters, plugins and database integration.
- SQLite: application persistence.
- JsonCpp: JSON handling.
- curl/OpenSSL: cloud license and provider HTTP calls.
- Gemini: AI provider, configured only through verified license metadata.
- Windows printing: `TxtPrinterService` invokes the platform print command.

`config.json` defines listeners, database, static files, upload limits, plugins
and service-specific settings. Secrets issued through licensing are not stored
in the public browser configuration.

## 7. Release invariants

A public Windows release must:

- be built from the tagged source commit with the documented static toolchain;
- depend only on Windows system DLLs;
- contain clean configuration, UI/static assets, bill template and end-user
  documentation, but no database, logs, generated bills or license cache;
- pass fresh cloud activation and HTTP startup checks from the packaged folder;
- ship as a versioned ZIP plus matching SHA-256 file;
- use an annotated tag that points to the released commit.

The authoritative procedure is
[READ_THIS_BEFORE_BUILDING_RELEASE_ASSETS.md](../READ_THIS_BEFORE_BUILDING_RELEASE_ASSETS.md).
