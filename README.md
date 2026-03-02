Latest version RuneLite launcher, with CI.

Can be used for any client revision (317, OSRS, 500+, etc.), as well as any Java application.

## Usage Guide

- Fork this repository on GitHub tester
- Replace all `Jirenyte` (case sensitive) with your server name.
- Replace all `jirenyte` (case sensitive) with your lowercase server name.
- Replace `jirenyte.com`/`Jirenyte.com` with your domain name.
- In `launcher.properties` replace the `https://static.runelite.net/bootstrap.json` link with a link to your own
  bootstrap (you can host it on a Gist for example).
  See [bootstrap.json.example](https://github.com/Jire/runelite-launcher/blob/main/bootstrap.json.example) for an
  example.
- Push your changes to GitHub, and then go to the Actions tab, click on the latest workflow.
- Wait for the workflow to finish, and then download the `jar`/`linux`/`macos-app`/`macos-dmg`/`windows` files and
  distribute them as you please.

## Support

If there's a problem with the launcher,
please [open an issue on GitHub](https://github.com/Jire/runelite-launcher/issues/new).

If you want it set up for you with blazing-fast edge-served hosting, contact **jire** on Discord.
