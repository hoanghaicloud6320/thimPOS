# AI API Contract

AI endpoints are stateless and require an authenticated account whose role is
`manager`. Authentication and authorization use `AuthNFilter` followed by
`AuthZFilter`.

## Generate content

`POST /api/ai/generate`

The client owns conversation state. To continue a conversation, it sends the
complete relevant message history with every request.

### Request

```json
{
  "messages": [
    { "role": "system", "content": "You are a retail analyst." },
    { "role": "user", "content": "Summarize today's sales." },
    { "role": "assistant", "content": "Previous answer, when continuing a chat." },
    { "role": "user", "content": "What should I do next?" }
  ],
  "temperature": 0.7
}
```

- `messages` is required and must contain 1–100 entries.
- Each message requires non-empty string fields `role` and `content`.
- Accepted roles are `system`, `user`, `assistant`, and `model` (`model` is
  normalized to `assistant`).
- Total message content is limited to 100,000 characters.
- `temperature` is optional, defaults to `0.7`, and must be between `0` and `2`.
- The API key and model cannot be supplied by the request. They are read only
  from verified license metadata fields `gem-apikey` and `gem-model-name`.

### Success — `200 OK`

```json
{
  "success": true,
  "data": {
    "model": "gemini-model-from-license",
    "message": {
      "role": "assistant",
      "content": "Generated text"
    }
  }
}
```

### Error shape

```json
{
  "success": false,
  "error": {
    "code": "ERROR_CODE",
    "message": "Human-readable details"
  }
}
```

| HTTP | Code | Meaning |
|---:|---|---|
| 400 | `INVALID_JSON` | Body is not a JSON object. |
| 400 | `AI_MESSAGES_REQUIRED` | Message history is absent or empty. |
| 400 | `INVALID_AI_MESSAGE` | A message is malformed or empty. |
| 400 | `INVALID_AI_ROLE` | A message has an unsupported role. |
| 400 | `INVALID_TEMPERATURE` | Temperature is outside 0–2. |
| 401 | Authentication filter error | Login/session is missing or invalid. |
| 403 | `FORBIDDEN` | The authenticated user is not a manager. |
| 413 | `AI_REQUEST_TOO_LARGE` | History exceeds the documented limits. |
| 502 | `AI_PROVIDER_ERROR` | Gemini/network response failed; details are forwarded from the service. |
| 503 | `AI_API_KEY_NOT_LICENSED` | `gem-apikey` is missing from license metadata. |
| 503 | `AI_MODEL_NOT_LICENSED` | `gem-model-name` is missing from license metadata. |
| 500 | `AI_INTERNAL_ERROR` | An unexpected controller error occurred. |

The backend does not store sessions or conversation history.

## Data APIs used by AI clients

All inventory endpoints below are manager-only and accept `limit` (1–500) and
non-negative `offset`:

- `GET /api/inventory/items?limit=500&offset=0&active_only=false`
- `GET /api/inventory/lots?from=<epoch>&to=<epoch>&limit=500&offset=0`
- `GET /api/inventory/recipes?product_id=<optional>&active_only=false&limit=500&offset=0`
- `GET /api/inventory/movements?from=<epoch>&to=<epoch>&kind=<optional>&limit=500&offset=0`

Movement kinds are `purchase`, `void_purchase`, `sale`, and
`sale_adjustment`. List responses use `{ items, limit, offset, from?, to? }`.

Orders use the existing endpoint:

`GET /api/orders?from=<epoch>&to=<epoch>&include_items=true&limit=500&offset=0`
