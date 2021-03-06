#ifndef _CHAT_BACKEND_H
#define _CHAT_BACKEND_H
#include "main.h"

#include <OS/OpenSpy.h>
#include <OS/Task.h>
#include <OS/Thread.h>
#include <OS/Mutex.h>

#include <vector>
#include <map>
#include <string>

#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#define _WINSOCK2API_
#include <stdint.h>
#include <hiredis/adapters/libevent.h>
#undef _WINSOCK2API_

#define CHAT_BACKEND_TICK 200

namespace Chat {
	class Driver;
	class Peer;
	extern redisContext *mp_redis_connection;

	struct _ChatQueryRequest;
	enum EChatBackendResponseError {
		EChatBackendResponseError_NoError,
		EChatBackendResponseError_NoUser_OrChan,
		EChatBackendResponseError_NoVoicePerms,
		EChatBackendResponseError_NoHOPPerms,
		EChatBackendResponseError_NoOPPerms,
		EChatBackendResponseError_NoOwnerPerms,
		EChatBackendResponseError_NickInUse,
		EChatBackendResponseError_NoChange,
		EChatBackendResponseError_BadPermissions,
		EChatBackendResponseError_NotOnChannel,
		EChatBackendResponseError_NoUserModeID,
		EChatBackendResponseError_NoChanProps,
		EChatBackendResponseError_Unknown,
	};
	typedef struct _ChatClientInfo {
		int client_id; //redis id
		std::string name;
		std::string user;
		std::string realname;
		std::string hostname;
		OS::Address ip;

		std::map<std::string, std::string> custom_keys;

		int profileid;
		int operflags;
	} ChatClientInfo;

	enum EChanClientFlags {
		EChanClientFlags_None = 0,
		EChanClientFlags_Voice = 1,
		EChanClientFlags_HalfOp = 2,
		EChanClientFlags_Op = 4,
		EChanClientFlags_Owner = 8,
		EChanClientFlags_Gagged = 16,
		EChanClientFlags_Banned = 32,
		EChanClientFlags_Invisible = 64,
		EChanClientFlags_Invited = 128, //or flood exempt for X chanmask
	};
	typedef struct _ChatChanClientInfo {
		uint32_t client_flags;
		int client_id;
		ChatClientInfo client_info;
		std::map<std::string, std::string> custom_keys; //setckey/getckey
	} ChatChanClientInfo;

	enum EChannelModeFlags {
		EChannelModeFlags_Private = 1,
		EChannelModeFlags_Secret = 2,
		EChannelModeFlags_TopicOpOnly = 4,
		EChannelModeFlags_NoOutsideMsgs = 8,
		EChannelModeFlags_Moderated = 16,
		EChannelModeFlags_Auditormium = 32,
		EChannelModeFlags_VOPAuditormium = 64,
		EChannelModeFlags_InviteOnly = 128,
		EChannelModeFlags_StayOpened = 256,
		EChannelModeFlags_OnlyOwner = 512, //only owner can change modes
	};

	typedef struct _ChatChannelInfo {
		int channel_id; //redis id
		std::string name;
		std::string topic;
		std::string topic_setby;
		int topic_seton;
		std::vector<int> clients; //client ids

		int modeflags;
		std::string password;
		int limit;

		std::string entry_msg;
		std::map<std::string, std::string> custom_keys; //setkey/getkey, also holds groupname(/setgroup)

		int chanprops_id;
	} ChatChannelInfo;

	typedef struct {
		EChanClientFlags mode_flag;
		ChatClientInfo client_info;
		bool set;
	} ChanClientModeChange;

	typedef struct {
		int id;
		std::string hostmask;
		std::string chanmask; //if * all channels, if X global(kline, etc)
		int modeflags;
		std::string comment;
		std::string setby;
		std::string machineid;
		int seton;
		int profileid;
		int setbypid;
		bool temporary; //delete when exit channel
		int expires; //can't be redis EXPIRE due to required events(unset modes, etc)
	} ChatStoredUserMode; 

	typedef struct {
		int id;
		std::string channel_mask; //if * all channels, if X global(kline, etc)
		int modeflags;
		std::string comment;
		std::string entrymsg;
		std::string setby;
		std::string topic;
		std::string password;
		int limit;
		int seton;
		int setbypid;
		int expires; //can't be redis EXPIRE due to required events(unset modes, etc)
		int kick_existing;
	} ChatStoredChanProps; 

	typedef struct _ChatQueryResponse {
		ChatChannelInfo channel_info;
		ChatClientInfo client_info;
		ChatChanClientInfo chan_client_info;
		std::vector<ChatChanClientInfo> m_channel_clients;

		std::vector< std::pair<EChatBackendResponseError, std::string> > errors;

		int operflags; //for get oper flags response

		ChatStoredChanProps channel_props_data;
		ChatStoredUserMode usermode_data;
		std::vector<ChatStoredUserMode> usermodes;
		std::vector<ChatStoredChanProps> chanprops;
	} ChatQueryResponse;


	typedef void (*ChatQueryCB)(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
	enum EChatQueryRequestType {
		EChatQueryRequestType_GetClientByName,
		EChatQueryRequestType_UpdateOrInsertClient,
		EChatQueryRequestType_SendClientMessage,
		EChatQueryRequestType_SendChannelMessage,
		EChatQueryRequestType_Find_OrCreate_Channel,
		EChatQueryRequestType_Find_Channel,
		EChatQueryRequestType_Find_ChannelByID,
		EChatQueryRequestType_AddUserToChannel,
		EChatQueryRequestType_RemoveUserFromChannel,
		EChatQueryRequestType_GetChannelUsers,
		EChatQueryRequestType_GetChannelUser,
		EChatQueryRequestType_UpdateChannelModes,
		EChatQueryRequestType_UpdateChannelTopic,
		EChatQueryRequestType_SetChannelClientKeys,
		EChatQueryRequestType_SetClientKeys,
		EChatQueryRequestType_SetChannelKeys,
		EChatQueryRequestType_KickUser, //send kick msg to all & remove user from channel, test perms 
		EChatQueryRequestType_UserDelete, //send quit msg to all

		EChatQueryRequestType_SaveUserMode,
		EChatQueryRequestType_SaveChanProps,
		EChatQueryRequestType_GetChanProps, //get all chan props
		EChatQueryRequestType_GetUserModes, //get all saved user modes(match by chanmask)
		EChatQueryRequestType_KillUser,
		EChatQueryRequestType_GetChatOperFlags,
		EChatQueryRequestType_DeleteUserMode,
		EChatQueryRequestType_DeleteChanProps,
		EChatQueryRequestType_GetClientUsermodes, //get all usermodes matching a client/chanmask combo
	};

	enum EChatMessageType {
		EChatMessageType_Msg,
		EChatMessageType_Notice,
		EChatMessageType_ATM,
		EChatMessageType_UTM,
	};

	enum EChannelPartTypes {
		EChannelPartTypes_Part,
		EChannelPartTypes_Kick,
	};

	typedef struct _ChatQueryRequest {
		EChatQueryRequestType type;
		ChatQueryCB callback;
		Peer *peer;
		void *extra;
		std::string query_name; //client/chan name

		EChatMessageType message_type;
		std::string message; //for client messages/topic

		ChatQueryResponse query_data;

		std::map<std::string, std::string> set_keys;

		//channel mode set stuff
		int add_flags;
		int remove_flags;
		std::string chan_password;
		int chan_limit;
		std::vector<std::pair<std::string, ChanClientModeChange> > user_modechanges;

		//disconnect data;
		ChatClientInfo client_info;
		std::vector<int> id_list;
		EChannelPartTypes part_reason;

	} ChatQueryRequest;


	typedef struct {
		int old_modeflags;
		int new_limit;
		std::string new_password;

		std::vector<ChanClientModeChange> client_modechanges;
	} ChanModeChangeData;

	enum EChannelPermissionType {
		EPermissionType_MustBeOnChannel,
		EPermissionType_VOPHigher,
		EPermissionType_HOPHigher,
		EPermissionType_OPHigher,
		EPermissionType_OwnerHigher,
	};

	class ChatBackendTask : public OS::Task<ChatQueryRequest> {
		public:
			ChatBackendTask();
			~ChatBackendTask();
			static ChatBackendTask *getQueryTask();
			static void Shutdown();

			void AddDriver(Chat::Driver *driver);
			void RemoveDriver(Chat::Driver *driver);

			static void SubmitGetClientInfoByName(std::string name, ChatQueryCB cb, Peer *peer, void *extra);
			static void SubmitClientInfo(ChatQueryCB cb, Peer *peer, void *extra);
			static void SubmitClientMessage(int target_id, std::string message, EChatMessageType message_type, ChatQueryCB cb, Peer *peer, void *extra);
			static void SubmitChannelMessage(int target_id, std::string message, EChatMessageType message_type, ChatQueryCB cb, Peer *peer, void *extra);
			static void SubmitFind_OrCreateChannel(ChatQueryCB cb, Peer *peer, void *extra, std::string channel);
			static void SubmitFindChannel(ChatQueryCB cb, Peer *peer, void *extra, std::string channel);
			static void SubmitFindChannelByID(ChatQueryCB cb, Peer *peer, void *extra, int id);

			static void SubmitAddUserToChannel(ChatQueryCB cb, Peer *peer, void *extra, ChatChannelInfo channel);
			static void SubmitRemoveUserFromChannel(ChatQueryCB cb, Peer *peer, void *extra, ChatChannelInfo channel, EChannelPartTypes reason, std::string reason_str);
			static void SubmitGetChannelUsers(ChatQueryCB cb, Peer *peer, void *extra, ChatChannelInfo channel);
			static void SubmitGetChannelUser(ChatQueryCB cb, Peer *peer, void *extra, ChatChannelInfo channel, std::string name);
			static void SubmitUpdateChannelModes(ChatQueryCB cb, Peer *peer, void *extra, uint32_t addmask, uint32_t removemask, ChatChannelInfo channel, std::string password, int limit, std::vector<std::pair<std::string, ChanClientModeChange> > user_modechanges, ChatClientInfo set_by);
			static void SubmitUpdateChannelTopic(ChatQueryCB cb, Peer *peer, void *extra, ChatChannelInfo channel, std::string topic);
			static void SubmitSetChannelKeys(ChatQueryCB cb, Peer *peer, void *extra, std::string channel, std::string user, const std::map<std::string, std::string> set_data_map);
			static void SubmitSetClientKeys(ChatQueryCB cb, Peer *peer, void *extra, int client_id, const std::map<std::string, std::string> set_data_map);
			static void SubmitSetChannelKeys(ChatQueryCB cb, Peer *peer, void *extra, ChatChannelInfo channel, const std::map<std::string, std::string> set_data_map);
			static void SubmitClientDelete(ChatQueryCB cb, Peer *peer, void *extra, std::string reason);
			static void SubmitGetChatOperFlags(int profileid, ChatQueryCB cb, Peer *peer, void *extra);
			static void SubmitSetSavedUserMode(ChatQueryCB cb, Peer *peer, void *extra, ChatStoredUserMode usermode);
			static void SubmitGetSavedUserModes(ChatQueryCB cb, Peer *peer, void *extra, ChatStoredUserMode usermode); //usermode used as search params
			static void SubmitDeleteSavedUserMode(ChatQueryCB cb, Peer *peer, void *extra, int id); //usermode used as search params

			static void SubmitSetChanProps(ChatQueryCB cb, Peer *peer, void *extra, ChatStoredChanProps chanprops);
			static void SubmitGetChanProps(ChatQueryCB cb, Peer *peer, void *extra, std::string mask);
			static void SubmitDeleteChanProps(ChatQueryCB cb, Peer *peer, void *extra, int id);

			static void SubmitGetClientUsermodes(ChatQueryCB cb, Peer *peer, void *extra, std::string chanmask, ChatClientInfo client);

			static void SubmitGetClientChannelUsermode(ChatQueryCB cb, Peer *peer, void *extra, std::string chanmask, ChatClientInfo client);

			static bool TestClientUsermode(ChatClientInfo client, ChatChannelInfo channel, ChatStoredUserMode usermode);
			static bool TestClientUsermode(ChatClientInfo client, ChatStoredUserMode usermode);

			static ChatStoredUserMode FlattenUsermodes(std::vector<ChatStoredUserMode> usermodes, ChatClientInfo client, std::string channel_mask = "X");
			void flagPushTask();
			static ChatClientInfo GetServerClient();
		private:
			static void *TaskThread(OS::CThread *thread);

			static void *setup_redis_async(OS::CThread *thread);

			static void onRedisMessage(redisAsyncContext *c, void *reply, void *privdata);

			void PerformGetClientInfoByName(ChatQueryRequest task_params);
			void PerformUpdateOrInsertClient(ChatQueryRequest task_params);
			void PerformSendClientMessage(ChatQueryRequest task_params);
			void PerformFind_OrCreateChannel(ChatQueryRequest task_params, bool no_create = false);
			void PerformFindChannelByID(ChatQueryRequest task_params);
			void PerformSendAddUserToChannel(ChatQueryRequest task_params);
			void PerformSendRemoveUserFromChannel(ChatQueryRequest task_params);
			void PerformGetChannelUsers(ChatQueryRequest task_params);
			void PerformUpdateChannelModes(ChatQueryRequest task_params);
			void PerformUpdateChannelTopic(ChatQueryRequest task_params);
			void PerformSendChannelMessage(ChatQueryRequest task_params);
			void PerformSetChannelClientKeys(ChatQueryRequest task_params);
			void PerformGetChannelUser(ChatQueryRequest task_params);
			void PerformSetClientKeys(ChatQueryRequest task_params);
			void PerformSetChannelKeys(ChatQueryRequest task_params);
			void PerformUserDelete(ChatQueryRequest task_params);
			void PerformGetChatOperFlags(ChatQueryRequest task_params);
			void PerformSaveUserMode(ChatQueryRequest task_params);
			void PerformGetSavedUserMode(ChatQueryRequest task_params);
			void PerformGetUserModes(ChatQueryRequest task_params);
			void PerformDeleteUserMode(ChatQueryRequest task_params);

			
			void PerformSetChanProps(ChatQueryRequest task_params);
			void PerformGetChanProps(ChatQueryRequest task_params);
			void PerformDeleteChanProps(ChatQueryRequest task_params);

			void PerformGetClientUsermodes(ChatQueryRequest task_params);

			bool TestChannelPermissions(ChatChanClientInfo chan_client_info, ChatChannelInfo channel_info, ChatQueryRequest task_params, struct Chat::_ChatQueryResponse &response);
			int GetUserChanPermissionScore(int client_flags);

			ChatChannelInfo GetChannelByName(std::string name);
			ChatChannelInfo GetChannelByID(int id);
			ChatChannelInfo CreateChannel(std::string name);
			ChatChanClientInfo GetChanClientInfo(int chan_id, int client_id);
			ChatStoredUserMode GetUserModeByID(int usermode_id);
			ChatStoredChanProps GetChanPropsByID(int chanprops_id);
			ChatStoredChanProps GetBestChanProps(std::string channel_mask);

			void GetChanPropsChannels(int chanprops_id, std::vector<ChatChannelInfo> &existing, std::vector<ChatChannelInfo> &newly_found);
			void ApplyChannelPropsToChannels(ChatStoredChanProps props, std::vector<ChatChannelInfo> channels);

			std::vector<ChatStoredUserMode> GetClientUsermodes(ChatClientInfo info, std::string channel_mask = "X");
			ChatStoredUserMode GetFlattenedUsermodes(ChatClientInfo info, std::string channel_mask = "X");
			void SendSetUsermodeToDrivers(ChatClientInfo remover, ChatStoredUserMode usermode);
			void SendDelUsermodeToDrivers(ChatClientInfo remover, ChatStoredUserMode usermode);

			ChatStoredChanProps GetChannelChanProps(std::string channel_name);
			ChatChannelInfo ApplyChannelPropsToChannel(ChatChannelInfo channel, ChatStoredChanProps props, bool send_mq = true);
			

			void LoadClientInfoByID(ChatClientInfo &info, int client_id);
			void LoadClientInfoByName(ChatClientInfo &info, std::string name);

			void SendClientMessageToDrivers(int target_id, ChatClientInfo user, const char *msg, EChatMessageType message_type);
			void SendChannelMessageToDrivers(ChatChannelInfo channel, ChatClientInfo user, const char *msg, EChatMessageType message_type);			
			void SendClientJoinChannelToDrivers(ChatClientInfo client, ChatChannelInfo channel);
			void SendClientPartChannelToDrivers(ChatClientInfo client, ChatChannelInfo channel, EChannelPartTypes part_reason, std::string reason_str);
			void SendChannelModeUpdateToDrivers(ChatClientInfo client_info, ChatChannelInfo channel_info, ChanModeChangeData change_data);
			void SendUpdateChannelTopicToDrivers(ChatClientInfo client, ChatChannelInfo channel);			
			void SendSetChannelClientKeysToDrivers(ChatClientInfo client, ChatChannelInfo channel, std::map<std::string, std::string> kv_data);
			void SendSetChannelKeysToDrivers(ChatClientInfo client, ChatChannelInfo channel, const std::map<std::string, std::string> kv_data);
			void SendUserQuitMessage(ChatClientInfo client, std::string quit_reason);

			static std::string ChannelInfoToKVString(ChatChannelInfo info);
			static ChatChannelInfo ChannelInfoFromKVString(const char *str);
			static std::string ClientInfoToKVString(ChatClientInfo info, std::string prefix = "");
			static ChatClientInfo ClientInfoFromKVString(const char *str, std::string prefix = "");
			static std::string UsermodeToKVString(ChatStoredUserMode info);
			static ChatStoredUserMode UsermodeFromKVString(const char *str);
			static std::string ChanPropsToKVString(ChatStoredChanProps info);
			static ChatStoredChanProps ChanPropsFromKVString(const char *str);

			void DeleteChannel(int channel_id, std::string reason = "Channel Deleted");

			std::vector<Chat::Driver *> m_drivers;
			redisContext *mp_redis_connection;
			redisContext *mp_redis_async_retrival_connection;
			redisAsyncContext *mp_redis_async_connection;
			struct event_base *mp_event_base;

			bool m_flag_push_task;

			OS::CThread *mp_async_thread;

			static ChatBackendTask *m_task_singleton;

			std::map<EChanClientFlags, std::string> m_permission_name_map;
	};
};

#endif //_MM_QUERY_H