## Comparison of Various Methods of Dotfiles Management

### Symlinking

- **Saves space** since files won't be duplicated.

- Edit-test cycles are **faster** because changes take effect immediately.

### Installing

- Configuration files can be automatically **generated** or programmatically
  **modified** during the install step. This can be used to implement
  host-specific configuration or construct extremely complex configurations.

- Programs that modify their own or other configuration files will not
  accidentally affect this source tree.

- Checkouts are **safer** because changes _don't_ take effect immediately.

### Not Source-Controlling it in the First Place

- Probably how dotfiles _should_ be managed.

- **Compatability** between different hosts isn't an issue.

