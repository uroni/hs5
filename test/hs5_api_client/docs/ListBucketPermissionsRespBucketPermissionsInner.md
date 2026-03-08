# ListBucketPermissionsRespBucketPermissionsInner


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**id** | **str** |  | 
**user_id** | **str** |  | 
**user_name** | **str** |  | 
**permissions** | **List[str]** |  | 

## Example

```python
from hs5_api.models.list_bucket_permissions_resp_bucket_permissions_inner import ListBucketPermissionsRespBucketPermissionsInner

# TODO update the JSON string below
json = "{}"
# create an instance of ListBucketPermissionsRespBucketPermissionsInner from a JSON string
list_bucket_permissions_resp_bucket_permissions_inner_instance = ListBucketPermissionsRespBucketPermissionsInner.from_json(json)
# print the JSON string representation of the object
print(ListBucketPermissionsRespBucketPermissionsInner.to_json())

# convert the object into a dict
list_bucket_permissions_resp_bucket_permissions_inner_dict = list_bucket_permissions_resp_bucket_permissions_inner_instance.to_dict()
# create an instance of ListBucketPermissionsRespBucketPermissionsInner from a dict
list_bucket_permissions_resp_bucket_permissions_inner_from_dict = ListBucketPermissionsRespBucketPermissionsInner.from_dict(list_bucket_permissions_resp_bucket_permissions_inner_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


