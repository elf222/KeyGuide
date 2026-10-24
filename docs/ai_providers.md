# AI tips and providers

KeyGuide can show a one-sentence tip beside a hovered shortcut ("when or why would I use
this?"), below or to the right of the key (**AI Tips ▸ Tip on Hover**). Tips come from an open-weight language model, either on your Mac through
[Ollama](https://ollama.com) or in the cloud through
[DigitalOcean Inference](https://docs.digitalocean.com/products/inference/). Both list models
through the OpenAI-compatible `/v1/models` route. DigitalOcean chats through
`/v1/chat/completions`. Ollama chats through its native `/api/chat` route, which, unlike the
OpenAI-compatible one, can raise the context window.

## How tips are generated

- **On hover.** Hovering a key with a hint asks for that shortcut plus up to seven others
  available in the same app state (three with Ollama). The hint shows `explaining...` until the tip arrives.
  After a failed request, hover requests pause for 30 seconds.
- **In bulk.** **AI Tips → Explain All Shortcuts** explains every remaining shortcut of one
  keymap in batches of 24 (8 with Ollama). Each item shows `explained/total`. **Stop Explaining** cancels.
- **Headless.** `KeyGuide --explain vim|blender|aerospace` does the same without a window
  or permissions, prints progress, writes the study guide and exits. Inside an app bundle,
  run `KeyGuide.app/Contents/MacOS/KeyGuide --explain vim`.

Tips are cached in `explanations.json` in the KeyGuide folder (**Open KeyGuide Folder**).
Each tip is keyed by keymap, context, keys and description, so editing a binding (for
example in `~/.aerospace.toml`) invalidates only that tip. **Clear Saved Tips** deletes
them all.

**AI Tips → Export Study Guide** writes `guides/<vim|blender|aerospace>.md`, with tables of
*Keys / What it does / Tip* grouped by context, and opens it.

## Grounding

The model never decides which shortcuts exist. KeyGuide sends a JSON list of shortcuts. Each
item has a batch id, the app, the keys, where the keys work and KeyGuide's own description.
The reply must be JSON. `src/guide/explanation_parser.cpp` then applies these rules:

1. An item must refer to an id that was sent, at most once.
2. Its text is converted to single-line ASCII (the overlay font has no other glyphs) and
   capped at 140 characters.
3. If the tip names a modifier shortcut (Ctrl/Cmd/Opt/Alt/Shift + key) that is neither this
   shortcut's own keys nor another shortcut of the same app, it is **rejected**. Modifier +
   mouse gestures such as `Shift-click` are allowed.

Rejected tips are counted in the AI Tips menu and in `--explain` output. The shortcut stays
unexplained, so the overlay never shows a hallucinated shortcut.

Only KeyGuide's built-in or parsed shortcut descriptions are sent, never typed text or
observed keystrokes.

## Ollama (local, default)

1. Install Ollama and keep it running (the menu-bar app, or `ollama serve`).
2. Pull the default model: `ollama pull qwen3.5:4b` (open-weight Qwen 3.5, 3.3 GB).
3. KeyGuide uses `qwen3.5:4b` unless you choose another model in **AI Tips → Model**. If the
   saved model is no longer installed (for example after `ollama rm`), KeyGuide switches to
   `qwen3.5:4b`, or else to the first model whose name does not contain `embed`, and says so
   in the AI Tips menu.

Ollama requests ask for JSON output (`format: json`), a 16k-token context window
(`num_ctx: 16384`) and no thinking (`think: false`, honoured by models whose thinking can be
switched off). With Ollama's default window, a reasoning model's thinking overflowed the
context and the reply was cut off mid-JSON. Measured on an M1 Pro (16 GB):

| Model | Result |
| --- | --- |
| `qwen3.5:4b` (default) | 81 of 85 AeroSpace tips in 73 s (`--explain aerospace`), none rejected by the grounding check; thinking is switched off |
| `huihui_ai/qwen3.5-abliterated:9b` | Answers without thinking too: one tip in 1.4 s after a 5 s load (6.6 GB) |
| `deepseek-r1:8b` | Good grounded tips, but it always reasons first: about 21 tokens/s, 36 s for three tips and about 2 minutes for a batch of eight |
| `qwen:4b` (Qwen 1.5) | Too old. It echoes the input instead of writing tips, so nothing passes the parser |

**AI Tips → Ollama Server…** points KeyGuide at another machine, for example a GPU Droplet
running Ollama, reached privately over Tailscale (`http://gpu-droplet:11434`).

## DigitalOcean Inference (cloud)

Serverless inference lives at `https://inference.do-ai.run/v1` and is authenticated with a
**model access key** sent as `Authorization: Bearer <key>`. KeyGuide's default model is
`openai-gpt-oss-120b`, OpenAI's Apache-2.0 open-weight model hosted by DigitalOcean.

### Setup

1. Sign in to DigitalOcean with billing enabled (apply hackathon credits if you have them).
2. Open **Model Access Keys**:
   <https://cloud.digitalocean.com/model-studio/manage-keys>.
3. Click **Create model access key**, then enter a name such as `keyguide`.
4. On the **Foundation models** tab choose **All models**, or **Select models** and pick the
   open-weight models you want (at least `openai-gpt-oss-120b`). Click **Continue**.
5. Under **VPC Network** choose **No VPC network**, then click **Add model access key**.
6. **Copy the secret now.** DigitalOcean shows it only once.
7. In KeyGuide, choose **AI Tips → Set DigitalOcean Access Key…**, paste it (Cmd-V works) and
   click **Save**. The key goes into your login Keychain (service `com.elf222.keyguide`).
   For `--explain` from a shell you can instead export
   `DIGITALOCEAN_MODEL_ACCESS_KEY=<key>`. The environment variable wins over the Keychain.
8. Choose **AI Tips → Provider → DigitalOcean Inference · cloud**.
9. Choose **AI Tips → Model**. Open-weight models are listed directly. DigitalOcean's
   proprietary models (Anthropic, OpenAI GPT-5 and similar) are grouped under
   **Proprietary (not open-weight)**. Stay with open-weight models for the
   *Best Open-Source AI Project* category.
10. Choose **AI Tips → Test Connection**. Expect
    `Connected to DigitalOcean: openai-gpt-oss-120b replied in … s`.

Check the key outside KeyGuide:

```sh
curl -s https://inference.do-ai.run/v1/models \
  -H "Authorization: Bearer $DIGITALOCEAN_MODEL_ACCESS_KEY" | head -c 400
```

### Troubleshooting

| Message | Meaning |
| --- | --- |
| `HTTP 401/403 - the API key was rejected` | Wrong key, a deleted key, or a key whose scope excludes the model. |
| `HTTP 404 - not found` | The model id is not available to this key; refresh the model list. |
| `Set a DigitalOcean model access key` | No key in the Keychain or environment. |
| `The model did not return the expected JSON` | Try another model; reasoning models can also need more time. |

Serverless inference is billed per token. One Vim keymap (about 450 shortcuts) is about 20
batch requests.

## Files

| File (in the KeyGuide folder) | Contents |
| --- | --- |
| `settings.json` | Provider, model per provider, Ollama address, tip placement (`below`, `right` or `off`), overlay choices |
| `explanations.json` | Cached tips with the model that wrote them |
| `guides/*.md` | Exported study guides |

The DigitalOcean key is never written to these files on macOS. Builds without a native
credential store keep it in an owner-only `secrets.json`.
