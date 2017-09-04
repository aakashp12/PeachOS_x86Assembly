
.data					# section declaration

        # Useful offset constants for accessing members of a 
        # struct mp1_blink_struct structure
        LOCATION   = 0    
        ON_CHAR    = 2
        OFF_CHAR   = 3 
        ON_LENGTH  = 4
        OFF_LENGTH = 6
        COUNTDOWN  = 8
        STATUS     = 10
        NEXT       = 12

        STRUCT_SIZE = 16

# Pointer to head of list (initialized to NULL)
mp1_list_head:
        .long   0

.text					# section declaration

# Export the function symbol names

.global mp1_rtc_tasklet
.global mp1_ioctl

# void mp1_poke(void);
#
# Interface: Register-based arguments (not C-style)
#    Inputs: %cl  - The byte you wish to write
#            %eax - Offset from the start of video memory that you wish
#                   to write to
#   Outputs: Text-mode video screen is written to at location %eax with
#            the byte in %cl
# Registers: Clobbers EDX
mp1_poke:
        
	movl    vmem_base_addr(,1),%edx
	movb    %cl,(%edx,%eax,1)
	ret

mp1_rtc_tasklet:
	ret

mp1_ioctl:
    pushl %ebp                          #push ebp to stack and save it
    movl %esp, %ebp
    movl 12(%ebp), %ecx                 # ecx <- ebp + 12
    cmpl $4, %ecx                       # if ecx > 4 then return -1  
    jae mp1_ioctl_other
    jmp *ioctl_jumpTable(, %ecx, 4)
        ret

mp1_ioctl_add:
    pushl %edi
    pushl %esi

    xorl %edi, %edi
    xorl %esi, %esi                     # clear them

    #     getting the TO address and checking for NULL
    pushl $16                           # size of struct
    movl %ebp, %esi
    call mp1_malloc                     # register EAX has a return value
    movl %eax, %edi
    addl 4(%esp)                        
    cmpl $0, %eax                       
    je mp1_add_return

    #     pushing arguments for mp1_copy_from_user and checking conditions
    pushl $16
    pushl 8(%esi)                       # check if FROM is NULL
    pushl %edi                          # push the return address from malloc 
    call mp1_copy_from_user
    addl 12(%esp)
    cmpl $0, %eax                       # compare return value of mp1_copy_from_user
    jne mp1_add_free
    
    #     countdown = on_lenght, status = 1
    movl %edi, %eax                     # return address(TO) -> eax
    movw 4(%eax), %di                   # di <- M[eax+4]
    movw %di, 8(%eax)                   # M[eax+8] = countdown = on_lenght
    movw $1, 10(%eax)                   # status is set to 1

    #   check for location, if its valid
    movw (%eax), %di                    # M[TO address] -> di 
    cmpw $1999, %di
    ja mp1_add_free                  

    #     now call mp1_poke
        ret

mp1_add_free:
    pushl %edi
    call mp1_free 
    addl 4(%esp)

mp1_add_return:
    movl $-1, %eax
        ret

mp1_ioctl_remove:
        ret

mp1_ioctl_find:
        ret

mp1_ioctl_sync:
        ret
mp1_ioctl_other:
    movl $-1, %eax
    leave
    ret
ioctl_jumpTable:
    .long mp1_ioctl_add, mp1_ioctl_remove, mp1_ioctl_find, mp1_ioctl_sync
.end