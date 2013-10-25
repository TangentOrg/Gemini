#define drFTL012 5183 /* User %i died holding %i shared memory locks. */
#define drFTL013 5184 /* User %i died with %i buffers locked. */
#define drMSG001 5185 /* %r%s is a void multi-volume database. */
#define drMSG012 5186 /* Multi-user session begin. */
#define drMSG013 5187 /* %s session end. */
#define drMSG020 5188 /* %s session begin for %s on %s. */
#define drMSG021 5189 /* Login by %s on %s. */
#define drMSG022 5190 /* Logout by %s on %s. */
#define drMSG026 5191 /* A previous prorest %s failed. */
#define drMSG046 5192 /* Open failed.  Database unusable due to partial con */
#define drMSG070 5193 /* Reduce -n, shared memory segment is too small. */
#define drMSG113 5194 /* WARNING: Before-image file of database %s is not t */
#define drMSG114 5195 /* The last backup was not completed successfully. */
#define drMSG119 5196 /* Backup terminated due to errors. */
#define drMSG123 5197 /* Read-only is not supported in this mode */
#define drMSG129 5198 /* Open failed, Database %s requires rebuild of all i */
#define drMSG133 5199 /* Physical Database Name (-db): %s. */
#define drMSG146 5200 /* Database Type (-dt): %s. */
#define drMSG159 5201 /* Force Access (-F): %s. */
#define drMSG162 5202 /* Direct I/O (-directio): %s. */
#define drMSG170 5203 /* Stopped. */
#define drMSG172 5204 /* User %i died holding %i shared memory locks. */
#define drMSG173 5205 /* User %i died with %i buffers locked. */
#define drMSG174 5206 /* Number of Database Buffers (gemini_buffer_cache):  */
#define drMSG175 5207 /* Disconnecting dead server %i. */
#define drMSG176 5208 /* Disconnecting client %i of dead server %i. */
#define drMSG177 5209 /* Disconnecting dead user %i. */
#define drMSG197 5210 /* %l is more than the maximum number temporary buffe */
#define drMSG205 5211 /* Switch to new ai extent failed. */
#define drMSG256 5212 /* Excess Shared Memory Size (-Mxs): %i. */
#define drMSG258 5213 /* Current Size of Lock Table (gemini_lock_table_size */
#define drMSG269 5214 /* The license statistics log file will be updated on */
#define drMSG276 5215 /* Hash Table Entries (-hash): %i. */
#define drMSG277 5216 /* Current Spin Lock Tries (-spin): %l. */
#define drMSG278 5217 /* Crash Recovery (-i): %s. */
#define drMSG279 5218 /* Delay of Before-Image Flush (-Mf): %i. */
#define drMSG281 5219 /* Before-Image File I/O (-r -R): %s. */
#define drMSG282 5220 /* Before-Image Truncate Interval (-G): %i. */
#define drMSG283 5221 /* Before-Image Cluster Size: %l. */
#define drMSG284 5222 /* Before-Image Block Size: %l. */
#define drMSG285 5223 /* Number of Before-Image Buffers (-bibufs): %l. */
#define drMSG287 5224 /* After-Image Stall (-aistall): %s. */
#define drMSG288 5225 /* After-Image Block Size: %l. */
#define drMSG289 5226 /* Number of After-Image Buffers (-aibufs): %l. */
#define drMSG290 5227 /* Maximum Number of Clients Per Server (-Ma): %i. */
#define drMSG291 5228 /* Maximum Number of Servers (-Mn): %i. */
#define drMSG292 5229 /* Minimum Clients Per Server (-Mi): %i. */
#define drMSG293 5230 /* Maximum Number of Users (-n): %i. */
#define drMSG294 5231 /* Host Name (-H): %s. */
#define drMSG295 5232 /* Service Name (-S): %s. */
#define drMSG296 5233 /* Network Type (-N): %s. */
#define drMSG297 5234 /* Character Set (-cpinternal): %s. */
#define drMSG299 5235 /* Server started by %s on %s. */
#define drMSG300 5236 /* Parameter File: %s. */
#define drMSG388 5237 /* Shared Memory Page Table Entry Optimization (-Mpte */
#define drMSG389 5238 /* Quiet point request login by %s on %s. */
#define drMSG392 5239 /* A quiet point has already been requested. */
#define drMSG393 5240 /* Enabling the quiet point is in effect. */
#define drMSG394 5241 /* After-imaging is enabled, this is not allowed with */
#define drMSG395 5242 /* Broker died during a quiet point enable request. */
#define drMSG396 5243 /* Quiet utility received shutdown request while atte */
#define drMSG397 5244 /* Quiet utility detected that the broker died during */
#define drMSG398 5245 /* Quiet utility received a shutdown request during q */
#define drMSG399 5246 /* Quiet point does not need to be disabled. */
#define drMSG400 5247 /* Quiet utility has detected that the broker is dead */
#define drMSG402 5248 /* Quiet point has been enabled by the broker. */
#define drMSG403 5249 /* Quiet point has been disabled by the broker. */
#define drMSG404 5250 /* Maximum Servers Per Broker (-Mpb): %i. */
#define drMSG405 5251 /* Minimum Port for Auto Servers (-minport): %i. */
#define drMSG406 5252 /* Maximum Port for Auto Servers (-maxport): %i. */
#define drMSG445 5253 /* Number of Semaphore Sets (-semsets) %l. */
#define drMSG448 5254 /* RL File Threshold Stall (-bistall): Enabled. */
#define drMSG449 5255 /* RL File Threshold Stall (-bistall): Disabled. */
#define drMSG451 5256 /* Request to change RL File Threshold value rejected */
#define drMSG452 5257 /* RL Threshold has not been reached and a Quiet Poin */
#define drMSG454 5258 /* Invalid RL Threshold value has been requested. */
#define drMSG457 5259 /* Forward processing stalled until database administ */
#define drMSG458 5260 /* Forward processing continuing. */
#define drMSG459 5261 /* Database Blocksize (-blocksize): %d. */
#define drMSG460 5262 /* Started using pid: %d. */
#define drMSG642 5263 /* Usr %l set name to %s. */
#define drMSG643 5264 /* %s failed with error exit %i. */
#define drMSG644 5265 /* %s called when database shutting down. rtc: %i. */
#define drMSG645 5266 /* %s called for invalid user. rtc: %i. */
#define drMSG660 5267 /* Storage object cache size (-omsize): %l  */
#define drMSG672 5268 /* Two-phase is enabled. */
#define drMSG680 5269 /* Refer to release notes for minor version %d before */
#define drMSG682 5270 /* RL File Threshold size (-bithold): %s. */
#define drMSG683 5271 /* RL File Threshold size of %s has been reached. */
#define drMSG684 5272 /* RL File size has grown to within 90 percent of the */
#define drMSG686 5273 /* RL File Threshold size has been changed from %s, t */
#define drMSG687 5274 /* RL File Threshold size is already %s. */
#define drMSG688 5609 /* Failed to open symbol file %s */
#define drMSG689 5610 /* Size of global transaction table (-maxxids): %l */
#define drMSG690 5611 /* Desired index block utilization invalid, changed f */
#define drMSG691 5612 /* Invalid Area #%i for object creation. */
#define drMSG692 5613 /* dsmObjectCreate failed to get admin lock %l */
#define drMSG693 5614 /* Created Table %s, id %d */
#define drMSG694 5615 /* Created Index %s, id %d on table %d */
